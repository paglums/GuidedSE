#include "symbolicexecutor.h"
#include "jsoncpp/dist/json/json.h"

#include <fstream>


#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_ostream.h>


#include <pthread.h>

int SymbolicExecutor::instances = 0;


SymbolicExecutor::SymbolicExecutor(std::string f, ServerSocket * s)
{
	socket = s;
	filename = f;
	instances++;
	currId = 0;
	BlockIds[NULL] = -1;
}

ExpressionTree* SymbolicExecutor::getExpressionTree(ProgramState* state, llvm::Value* value)
{
	if (ExpressionTree* exptree = state->get(value))
	{
		return exptree;
	}
	else return new ExpressionTree(value);
}	
void SymbolicExecutor::executeNonBranchingInstruction(llvm::Instruction* instruction,ProgramState* state)
{
	#ifdef DEBUG	
		std::cout << " executing :" << instruction->getOpcodeName() << " instruction \n";
	#endif
	if (instruction->getOpcode() == llvm::Instruction::Alloca)
	{
		#ifdef DEBUG	
			std::cout << "executing Store \n";
		#endif
		//state->variables.insert(std::make_pair(getString(instruction).c_str(),state->c.int_const(getString(instruction).c_str())));
		//state->variables.insert(std::pair<std::string, z3::expr>(getString(instruction).c_str(),state->c.int_const(getString(instruction).c_str())));
		//std::cout << state->variables.at(getString(instruction).c_str()) << std::endl;
		//state->variables[getString(instruction).c_str()] = state->c.int_const(getString(instruction).c_str());		
	}
	else if(instruction->getOpcode()==llvm::Instruction::Store)
	{
		#ifdef DEBUG	
			std::cout << "executing Store \n";
		#endif
		llvm::Value* memLocation = instruction->getOperand(1);
		llvm::Value* value = instruction->getOperand(0);
		state->add(memLocation,getExpressionTree(state,value));
		//state->s->add(state->variables[getString(memLocation).c_str()] == state->variables[getString(value).c_str()]);
	}
	else if(instruction->getOpcode()==llvm::Instruction::Load)
	{
		#ifdef DEBUG	
			std::cout << "executing Load \n";
		#endif
		ExpressionTree* exptree = getExpressionTree(state,instruction->getOperand(0));
		state->add(instruction,exptree);
		//state->variables.insert(std::pair<std::string, z3::expr>(getString(instruction).c_str(),state->c.int_const(getString(instruction).c_str())));
		 // state->s->add(state->variables[getString(instruction).c_str()] == state->variables[getString(instruction->getOperand(0)).c_str()]);

		#ifdef DEBUG	
		if(!exptree)
			std::cout << "expression tree not found \n";
		#endif 
	}
	else if(instruction->getOpcode()==llvm::Instruction::Add)
	{
		#ifdef DEBUG	
			std::cout << "executing Add \n";
		#endif

		ExpressionTree* lhs = getExpressionTree(state,instruction->getOperand(0));
		ExpressionTree* rhs = getExpressionTree(state,instruction->getOperand(1));
	
		#ifdef DEBUG
			if (lhs)
				std::cout << "lhs not NULL\n";
			if (rhs)
				std::cout << "rhs not NULL\n";

			std::cout << "lhs: " << lhs->toString(state->getMap()) <<"\n";
			std::cout << "rhs: " << rhs->toString(state->getMap()) <<"\n";
		#endif

		state->add(instruction,new ExpressionTree("+",lhs,rhs));	
		//std::cout << state->variables.at(getString(instruction->getOperand(0)).c_str()) + state->variables.at(getString(instruction->getOperand(1)).c_str())	<< std::endl; 
		//state->variables[getString(instruction).c_str()] = state->c.bool_const(getString(instruction).c_str());
		//state->s->add(state->variables[getString(instruction).c_str()] == state->variables[getString(instruction->getOperand(0)).c_str()] + state->variables[getString(instruction->getOperand(1)).c_str()]);	 
	}
	else if (instruction->getOpcode() == llvm::Instruction::ICmp)
	{
		#ifdef DEBUG	
			std::cout << "executing ICmp \n";
		#endif

		llvm::ICmpInst* cmpInst = llvm::dyn_cast<llvm::ICmpInst>(instruction); 
		// if (llvm::ConstantInt* cl = llvm::dyn_cast<llvm::ConstantInt>(value))
		if(cmpInst->getSignedPredicate() == llvm::ICmpInst::ICMP_SGT)
		{
			ExpressionTree* lhs = getExpressionTree(state,instruction->getOperand(0));
			ExpressionTree* rhs = getExpressionTree(state,instruction->getOperand(1));
			state->add(instruction,new ExpressionTree(">",lhs,rhs));
			//state->variables[getString(instruction).c_str()] = state->c.bool_const(getString(instruction).c_str());
			//state->s->add(state->variables[getString(instruction).c_str()] == state->variables[getString(instruction->getOperand(0)).c_str()] > state->variables[getString(instruction->getOperand(1)).c_str()]);
		}
	}
	#ifdef DEBUG
		std::cout << "exiting executeNonBranchingInstruction\n";
	#endif
}




/**
 Executes a branching instruction and determines which block(s) need to be explored depending on the program state
*/
std::vector<std::pair<llvm::BasicBlock*, ProgramState*>> 
	SymbolicExecutor::getNextBlocks(llvm::Instruction* inst, ProgramState* state)
{
	std::vector<std::pair<llvm::BasicBlock*, ProgramState*> > pairs;
	llvm::Value* value = NULL;
	if(inst->getOpcode() == llvm::Instruction::Ret)
	{
		return pairs;
	}
	else
	{
		if (!llvm::isa<llvm::BasicBlock>(inst->getOperand(0)))
		{
			value = llvm::dyn_cast<llvm::Value> (inst->getOperand(0));
			//state->s->add(state->variables.at(getString(value).c_str()) == true);
			state->constraints.push_back(std::pair<llvm::Value*, std::string>(value, "true"));
		}

		llvm::Value * check = NULL;
		ExpressionTree * check_expr;

		for (int j = 0; j < inst->getNumOperands(); j++)
		{
			// std::cout <<"operand no : " << j+1 << "\n" << getString(inst->getOperand(j)) << "\n";
			llvm::BasicBlock* basicBlock = NULL;
			if (llvm::isa<llvm::BasicBlock>(inst->getOperand(j)))
			{
				basicBlock = llvm::dyn_cast<llvm::BasicBlock> (inst->getOperand(j));
			}
			llvm::Value* v = dynamic_cast<llvm::Value*> (inst->getOperand(j));
			if (!basicBlock && j == 0)
			{
				// std::cout << "yy!!\n";
				check = v;
				check_expr = state->get(check);
			}
			if (basicBlock) 
			{
				ProgramState* prg = new ProgramState(*state);
				if (value)
				{
					if (j <= 1)
					{
						prg->constraints.push_back(std::pair<llvm::Value*, std::string>(value, "true"));
						prg->addCondition(state->get(value)->toString(state->getMap()));

						//prg->s->add(prg->variables.at(getString(value).c_str()) == true);
					}
					else
					{
						prg->constraints.push_back(std::pair<llvm::Value*,std::string>(value, "false"));
						prg->addCondition("not " + state->get(value)->toString(state->getMap()));

						//prg->s->add(prg->variables.at(getString(value).c_str()) == false);
					}

					pairs.push_back(std::make_pair(basicBlock, prg));
				}
				else
					pairs.push_back(std::make_pair(basicBlock, state)); 
			}
		}

		/*if (check && check_expr->isConstant())
		{
			if (to_ret.size() > 1)
			{
			if (check_expr->getInteger() == 0)
			{
				to_ret.resize(1);
				#ifdef DEBUG
					std::cout << "here 1\n";
				#endif
				return to_ret;
			}
			else if (check_expr->getInteger() == 1) 
			{
				to_ret[0] = to_ret[1];
				to_ret.resize(1);
				#ifdef DEBUG
					std::cout << "here 2\n";
				#endif
				return to_ret;
			}
			}

		}
		*/
	}
	#ifdef DEBUG
	// std::cout << "about to return possible branches as follows:" << std::endl;
	// for (int j = 0; j < to_ret.size(); j++)
	// {
	// auto block = to_ret[j];
	// if (block)
		// std::cout << "new block not null" << std::endl;
	// for (auto i = block->begin(), e = block->end(); i != e; ++i)
	// {
		// printf("Basic block (name= %s) has %zu instructions\n",block->getName().str().c_str(),block->size());

		// std::cout << "printing operands ";
		// for (int j = 0; j < i->getNumOperands(); j++)
		// {
		//	 std::cout << getString(i->getOperand(j)) << "\n";
		// }
		// std::cout << getString(i) << "\n";
		// std::cout << (*i).print();
		// The next statement works since operator<<(ostream&,...)
		// is overloaded for Instruction&
		// std::cout << *(i).str().c_str() << "\n";
		// std::cout << "move forward? ";
		// int x;
		// std::cin >> x;	
	// }
	// }


	std::cout << "exiting getNextBlocks\n";

	#endif

	return pairs;
}


/**
 executes the basicBlock, updates programstate and returns the next Block(s) to execute if it can be determined that only the "Then" block should be executed then only the "Then" block is returned. Similarly for the else block. Otherwise both are are retuarned. NULL is returned if there's nothing left to execute
 */
std::vector<std::pair<llvm::BasicBlock*, ProgramState*> > 
	SymbolicExecutor::executeBasicBlock(llvm::BasicBlock* block, ProgramState* state)
{
	std::vector<std::pair<llvm::BasicBlock*, ProgramState*>> to_ret;
	#ifdef DEBUG
	printf("Basic block (name= %s) has %zu instructions\n",block->getName().str().c_str(),block->size());
	#endif

	for (auto i = block->begin(), e = block->end(); i != e; ++i)
	{
		#ifdef DEBUG

			std::cout << "printing operands : " << i->getNumOperands() << "\n";
			for (int j = 0; j < i->getNumOperands(); j++)
			{
				std::cout << "operand # : " << j+1 << " : "<< getString(i->getOperand(j)) << "\n";
			}
			std::cout << "printing instruction: " << getString(i) << "\n";
			std::cout << "getOpcode: " << i->getOpcode() << "\n";
			std::cout << "move forward? \n";
			// std::cout << llvm::Instruction::Ret << "\n";
			// int x;
			// std::cin >> x;
		#endif

		if(i->getOpcode() == llvm::Instruction::Br || i->getOpcode() == llvm::Instruction::Ret) 
		{
			#ifdef DEBUG
				std::cout << "Branch Instruction Hit!\n";
				// std::cin >> x;
				// std::cout << "about to return!";
			#endif
			return getNextBlocks(i,state);
		}
		else 
		{
			#ifdef DEBUG
				int abc;
				std::cout << "non branch instruction to b executed\n";
				// std::cin >> abc;
				if (i)
				{ 
					std::cout << getString(i) << "\n" << std::endl;
				}
				else
				{
					std::cout << "instruction NULL\n"<< std::endl;
				}
				// std::cin >> abc;
			#endif

			executeNonBranchingInstruction(i,state);
		}
			#ifdef DEBUG
				std::cout << "Instruction Executed! (either branch or non branch)\n";
				// std::cin >> x;
			#endif
	}

	#ifdef DEBUG
		std::cout << "exiting executeBasicBlock\n";
	#endif

	return to_ret;
}

void SymbolicExecutor::symbolicExecute(ProgramState * s, llvm::BasicBlock * prev, 
									llvm::BasicBlock * b, std::vector<ProgramState*> & vec)
{
	std::vector<std::pair<llvm::BasicBlock*, ProgramState*>> new_blocks = executeBasicBlock(b,s);
	Json::Value msg;
	BlockIds[b]=currId++;
	msg["fileId"] = Json::Value(filename.c_str());
	msg["node"] = Json::Value(BlockIds[b]);
	if(prev != NULL) msg["parent"] = Json::Value(BlockIds[prev]);
	msg["text"] = Json::Value(s->toString());
	msg["fin"] = Json::Value("0");
	msg["constraints"] = Json::Value(s->getPathCondition());
	Json::FastWriter fastWriter;
	std::string output = fastWriter.write(msg);
	std::cout << "sending this: " << output << std::endl;
	(*socket) << output;
	std::cout << "going to sleep " << std::endl;

	

	// sigemptyset (&mask);
	// sigaddset (&mask, SIGUSR1);
	// sigprocmask (SIG_BLOCK, &mask, &oldmask);
	// sigsuspend (&oldmask);
	{
	std::unique_lock<std::mutex> lck(mtx);
	cv.wait(lck);
	}

	// sigfillset(&mask);
	// sigdelset(&mask, SIGRTMIN+instances);
	// sigsuspend(&mask);
	std::cout << "wakeup!! " << std::endl;


	if (new_blocks.size() < 1)
	{
		vec.push_back(s);
		return;
	}
	for (int i = 0; i < new_blocks.size(); i++)
	{
		if (new_blocks[i].first)
		{
			#ifdef DEBUG
			std::cout << "new block not null" << std::endl;
			#endif
			// std::cout << "************(input) printing vars!**********\n" << std::endl;
			// std::cout << s->printVariables();
			// std::cout << "************(input) printing vars!**********\n" << std::endl;
			// std::cout << "************(output) printing vars!**********\n" << std::endl;
			// t->printVariables();
			// std::cout << "************(output) printing vars!**********\n" << std::endl;
			symbolicExecute(new_blocks[i].second, b, new_blocks[i].first, vec);
		}

		#ifdef DEBUG
			else
			{
				std::cout << "new block NULL!!" << std::endl;
			}
		#endif

	}
}


/**
	Executes all the possible paths in the given function and returns the programState at the end of every path
*/
std::vector<ProgramState*> SymbolicExecutor::executeFunction(llvm::Function* function)
{
	std::vector<ProgramState*> vec;
	ProgramState* state = new ProgramState(function->args());
	#ifdef DEBUG
		std::cout << state->toString();
	#endif
	llvm::BasicBlock* currBlock;
	// std::cout << "good to go!" << std::endl;
	std::vector<llvm::BasicBlock*> blocks;
	blocks.push_back(&function->getEntryBlock());
	//since we're only writing code for executing a single path we can simply do this
	symbolicExecute(state, NULL, &function->getEntryBlock(), vec);
	Json::Value msg;
	msg["fin"] = Json::Value("1");
	Json::FastWriter fastWriter;
	std::string output = fastWriter.write(msg);

	std::cout << "sending this: " << output << std::endl;
	(*socket) << output;
	std::cout << "Function executed"	<< std::endl;

	/***
	while(blocks.size())
	{
		currBlock = blocks[0];
		blocks.erase(blocks.begin());
		std::vector<llvm::BasicBlock*> new_blocks = executeBasicBlock(currBlock,state);
		// std::cout << "block executed" << std::endl;
		// std::cout << "new blocks received:	" << blocks.size() << std::endl;
		for (int i = 0; i < new_blocks.size(); i++)
		{
		if (blocks[i])
		{
			blocks.push_back(new_blocks[i]);
			#ifdef DEBUG
			std::cout << "new block not null" << std::endl;
			#endif
		}
		else
		{
			#ifdef DEBUG
			std::cout << "new block NULL!!" << std::endl;
			#endif
		}
		}
		#ifdef DEBUG
		std::cout << "new blocks added!!" << blocks.size() << std::endl;
		#endif

		// int y;
		// std::cin >> y;
		// if(blocks.size() > 0) currBlock = blocks[0];
	}
	// std::cout << state->getPathCondition();
	***/
	
	return vec;
	
}


llvm::Module* SymbolicExecutor::loadCode(std::string filename) 
{
	auto Buffer = llvm::MemoryBuffer::getFileOrSTDIN(filename.c_str());
	if(!Buffer)
	{
		printf("not Buffer\n");
	}
	auto mainModuleOrError = getLazyBitcodeModule(Buffer->get(), llvm::getGlobalContext());
	if(!mainModuleOrError)
	{
		printf("not mainModuleOrError\n");
	}
	else 
	{
		// The module has taken ownership of the MemoryBuffer so release it
		// from the std::unique_ptr
		Buffer->release();
	}
	(**mainModuleOrError).materializeAllPermanently();
	return *mainModuleOrError;
}

void SymbolicExecutor::proceed()
{
	std::cout << "WAKE UP!!!!!" << std::endl;
	std::unique_lock<std::mutex> lck(mtx);
	cv.notify_all();
	std::cout << "AWAKEN UP!!!!!" << std::endl;
	// sigprocmask (SIG_UNBLOCK, &mask, NULL);
	// sigprocmask (SIG_UNBLOCK, &mask, NULL);
}


void SymbolicExecutor::execute()
{
	llvm::Module* module = loadCode(filename.c_str());
	// for (auto function = module->begin(), last = module->end(); function!=last; function++)
	// {
	//		 printf("%s\n",function->getName().str().c_str());
	// }
	auto function = module->getFunction("_Z7notmainii");
	std::vector<ProgramState*> final_states = executeFunction(function);
	std::cout << "final states: ("<< final_states.size() << ")\n";
	for (int i = 0; i < final_states.size(); i++)
	{
		std::cout << final_states[i]->toString() << "\n\n" ;
	}
	for (int i = 0; i < final_states.size(); i++)
	{
		final_states[i]->printZ3Variables();
		std::cout << "\n";
		final_states[i]->Z3solver();
		std::cout << "\n\n";
	}
}
// int main()
// {
//	 std::cout << "good to go!" << std::endl;
//	 return 0;
// }