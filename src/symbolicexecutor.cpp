#include "symbolicexecutor.h"
#include "jsoncpp/dist/json/json.h"
#include "utils.h"
#include <fstream>
#include "messagetypes.h"


#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CallSite.h>
#include <pthread.h>


int SymbolicTreeNode::instances = 0;

SymbolicExecutor::SymbolicExecutor(std::string f, ServerSocket * s)
{
	socket = s;
	filename = f;
	rootNode = NULL;
    isBFS = false;
    dir = 0;
    steps = -1;
    prevId = -1;
}

ExpressionTree* SymbolicExecutor::getExpressionTree(ProgramState* state, llvm::Value* value)
{
	if (ExpressionTree* exptree = state->get(value))
	{
		return exptree;
	}
	else return new ExpressionTree(value);
}	
void SymbolicExecutor::executeNonBranchingInstruction(llvm::Instruction* instruction,SymbolicTreeNode* symTreeNode)
{
	ProgramState* state = symTreeNode->state;
	#ifdef DEBUG	
		std::cout << " executing :" << instruction->getOpcodeName() << " instruction \n";
		std::cout << "State at this point == \n------------------" << state->toString() << "\n";
	#endif

	
	if (instruction->getOpcode() == llvm::Instruction::Alloca)
	{
	}
	else if(instruction->getOpcode()==llvm::Instruction::Store)
	{
		llvm::Value* memLocation = instruction->getOperand(1);
		llvm::Value* value = instruction->getOperand(0);
		state->add(memLocation,getExpressionTree(state,value));
	}
	else if(instruction->getOpcode()==llvm::Instruction::Load)
	{
		ExpressionTree* exptree = getExpressionTree(state,instruction->getOperand(0));
		state->add(instruction,exptree);
	
		#ifdef DEBUG	
		if(!exptree)
			std::cout << "expression tree not found \n";
		#endif 
	}
	else if(instruction->getOpcode()==llvm::Instruction::Add)
	{
		ExpressionTree* lhs = getExpressionTree(state,instruction->getOperand(0));
		ExpressionTree* rhs = getExpressionTree(state,instruction->getOperand(1));
	
		#ifdef DEBUG
			if (lhs)
				std::cout << "lhs not NULL\n";
			if (rhs)
				std::cout << "rhs not NULL\n";

			std::cout << "lhs: " << lhs->toString() <<"\n";
			std::cout << "rhs: " << rhs->toString() <<"\n";
		#endif

		state->add(instruction,new ExpressionTree("+",lhs,rhs));		
	}
	else if (instruction->getOpcode() == llvm::Instruction::ICmp)
	{
		llvm::ICmpInst* cmpInst = llvm::dyn_cast<llvm::ICmpInst>(instruction); 
		std::string op;
		if(cmpInst->getSignedPredicate() == llvm::ICmpInst::ICMP_SGT)
			op = ">";	
		else if(cmpInst->getSignedPredicate() == llvm::ICmpInst::ICMP_SLT)
			op = "<";
		else if(cmpInst->getSignedPredicate() == llvm::ICmpInst::ICMP_SLE)
			op = "<=";
		#ifdef DEBUG
			std::cout << "operand0 == " << getString(instruction->getOperand(0)) << "\n";
			std::cout << "operand1 == " << getString(instruction->getOperand(1)) << "\n";
		#endif
		ExpressionTree* lhs = getExpressionTree(state,instruction->getOperand(0));
		ExpressionTree* rhs = getExpressionTree(state,instruction->getOperand(1));
		#ifdef DEBUG
			if(lhs) std::cout << "lhs == " << lhs->toString() << "\n";
			else std::cout << "lhs is NULL\n";
			if(rhs) std::cout << "rhs == " << rhs->toString() << "\n";
			else std::cout << "rhs is NULL\n";
		#endif
		state->add(instruction,new ExpressionTree(op,lhs,rhs));
		#ifdef DEBUG
			std::cout << "state updated\n";
		#endif
			//state->variables[getString(instruction).c_str()] = state->c.bool_const(getString(instruction).c_str());
			//state->s->add(state->variables[getString(instruction).c_str()] == state->variables[getString(instruction->getOperand(0)).c_str()] > state->variables[getString(instruction->getOperand(1)).c_str()]);
	}
	#ifdef DEBUG
		std::cout << "exiting executeNonBranchingInstruction\n";
	#endif
}




/**
 Executes a branching instruction and determines which block(s) need to be explored depending on the program state
*/
std::vector<SymbolicTreeNode*> 
	SymbolicExecutor::getNextBlocks(llvm::Instruction* inst, SymbolicTreeNode* node)
{
	std::vector<SymbolicTreeNode*> children;
	ProgramState* state = node->state;
	int pid = node->id;
	if(llvm::isa<llvm::BranchInst>(inst))
	{
		llvm::BranchInst* binst = llvm::dyn_cast<llvm::BranchInst>(inst);
		if(binst->isConditional())
		{
			ProgramState* first = new ProgramState(*state);
			llvm::Value* cond = binst->getCondition();
			int toAddTrue = 1;
			int toAddFalse = 1;
			if(state->getMap()[cond]->isConstant())
			{
				toAddTrue = state->getMap()[cond]->getInteger();
				toAddFalse = !toAddTrue;
			}
			if(toAddTrue)
			{
				first->constraints.push_back(std::make_pair(cond,"true"));
				first->addCondition(state->get(cond)->toString());
				std::cout << "ADDING CONDITION : " 
						 << state->get(cond)->toString() << std::endl;
				children.push_back(new SymbolicTreeNode(binst->getSuccessor(0), first, pid,NULL,NULL,node));
			}
			int numSuccesors = binst->getNumSuccessors();
			if(numSuccesors == 2 && toAddFalse)
			{
				ProgramState* second = new ProgramState(*state);
				second->constraints.push_back(std::make_pair(cond,"false"));
				second->addCondition("not " + state->get(cond)->toString());
				children.push_back(new SymbolicTreeNode(binst->getSuccessor(1), second, pid,NULL,NULL,node));
			}
		}
		else children.push_back(new SymbolicTreeNode(binst->getSuccessor(0),state, pid,NULL,NULL,node));
	}
	else if (llvm::isa<llvm::CallInst>(inst))
	{
		llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(inst);
		llvm::Function* calledFunction = callInst->getCalledFunction();
		if(calledFunction->isDeclaration())
		{
			#ifdef DEBUG
				std::cout << "EXTERNAL CALL\n";
			#endif

			Json::Value msg;
			msg["node"] = Json::Value(symTreeNode->id);
			msg["parent"] = Json::Value(symTreeNode->prevId);
			msg["external"] = Json::Value("true");
			msg["fin"] = Json::Value("0");
			
			sendMessageAndSleep(msg);
			
		}
		else
		{
			std::cout << "INTERNAL CALL\n";
			std::cout << getString(callInst->getCalledValue());
			llvm::BasicBlock* funcStart = &calledFunction->getEntryBlock();
			std::vector<ExpressionTree*> arguments;
			//llvm::CallSite* callsite = llvm::dyn_cast<llvm::CallSite>(callInst);
			for(auto arg = callInst->op_begin(); arg != callInst->op_end(); arg++)
			{
				//llvm::Value* val_arg = llvm::dyn_cast<llvm::Value>(arg);
				arguments.push_back(state->get(arg->get()));
			}
			ProgramState* newState = new ProgramState(calledFunction->args(),arguments);
			ProgramState::Copy(*state,newState,false);
			children.push_back(new SymbolicTreeNode(funcStart,newState, pid,NULL,NULL,NULL,node));
		} 
		
	}
	else if (llvm::isa<llvm::ReturnInst>(inst))
	{
		if(node->parent)
		{
			llvm::ReturnInst* returninst = llvm::dyn_cast<llvm::ReturnInst>(inst);
			SymbolicTreeNode* parent = node->parent;
			InstructionPtr* returnptr = new InstructionPtr(parent->getPreviousInstruction());
			ProgramState* newState = new ProgramState(*(parent->state)); // copy everything from parent state
			ProgramState::Copy(*state,newState,false); // replace everything except map from curr state
			newState->add(*returnptr,node->state->get(returninst->getReturnValue()));
			(*returnptr)++;
			children.push_back(new SymbolicTreeNode(parent->block,newState,
								parent->id,returnptr,
								NULL,NULL,parent->parent));	
		}
		
	}

	#ifdef DEBUG
		std::cout << "exiting getNextBlocks\n";
	#endif

	return children;
}

bool isSplitPoint(llvm::Instruction* instruction)
{
	return instruction->getOpcode() == llvm::Instruction::Br 
		   || instruction->getOpcode() == llvm::Instruction::Ret
		   || ( llvm::isa<llvm::CallInst>(instruction) 
				&& !llvm::isa<llvm::DbgDeclareInst>(instruction) );
}
/**
 executes the basicBlock, updates programstate and returns the next Block(s) to execute if it can be determined that only the "Then" block should be executed then only the "Then" block is returned. Similarly for the else block. Otherwise both are are retuarned. NULL is returned if there's nothing left to execute
 */
std::vector<SymbolicTreeNode*>
	SymbolicExecutor::executeBasicBlock(SymbolicTreeNode* symTreeNode)
{
	ProgramState* state = symTreeNode->state;
	llvm::BasicBlock* block = symTreeNode->block;
	
	std::vector<SymbolicTreeNode*> to_ret;
	
	#ifdef DEBUG
		printf("Basic block (name= %s) has %zu instructions\n"
				,block->getName().str().c_str(),block->size());
	#endif

	while(symTreeNode->hasNextInstruction())
	{
		auto instruction = symTreeNode->getNextInstruction();
		#ifdef DEBUG

			std::cout << "printing operands : " << instruction->getNumOperands() << "\n";
			for (int j = 0; j < instruction->getNumOperands(); j++)
			{
				std::cout << "operand # : " << j+1 << " : " << getString(instruction->getOperand(j)) << "\n";
			}
			std::cout << "printing instruction: " << getString(instruction) << "\n";
			std::cout << "getOpcode: " << instruction->getOpcode() << "\n";

		#endif

		if (llvm::MDNode *N = instruction->getMetadata("dbg")) 
		{  
			llvm::DILocation Loc(N);
			unsigned Line = Loc.getLineNumber();
			symTreeNode->minLineNumber = std::min(symTreeNode->minLineNumber,Line);
			symTreeNode->maxLineNumber = std::max(symTreeNode->maxLineNumber,Line);
			std::cout << "instruction == " << getString(instruction) << "\tLine Number == " << Line << "\n";
		}

		if(isSplitPoint(instruction)) 
		{
			#ifdef DEBUG
				std::cout << "Split Point Hit!\n";	
			#endif

			return getNextBlocks(instruction,symTreeNode);
		}
		else 
		{
			#ifdef DEBUG
				std::cout << "non branch instruction to b executed\n";
				if (instruction)
				{ 
					std::cout << getString(instruction) << "\n" << std::endl;
				}
				else
				{
					std::cout << "instruction NULL\n"<< std::endl;
				}
			#endif

			executeNonBranchingInstruction(instruction,symTreeNode);
		}
	}

	#ifdef DEBUG
		std::cout << "exiting executeBasicBlock\n";
	#endif

	return to_ret;
}



void SymbolicExecutor::symbolicExecute()
{
	
	while (1)
	{
		Json::Value toSend;
		toSend["type"] = Json::Value(MSG_TYPE_EXPANDNODE);
		toSend["fileId"] = Json::Value(filename.c_str());

		int currObject = 0;
		#ifdef DEBUG
			std::cout << "prev id: "<< prevId << "\n";
		#endif
		std::deque<SymbolicTreeNode*> deque;
		if (prevId == -1)
		{
			deque.push_back(rootNode);
		}
		else
		{
			SymbolicTreeNode * symTreeNode = BlockStates[prevId];

			if (!symTreeNode)
			{
				#ifdef DEBUG
					std::cout << "tree node is NULL!!\n"; 
				#endif
			}

			if (symTreeNode->left)
			{
				#ifdef DEBUG
					std::cout << "left child started!!\n";
				#endif

				if (dir)
					deque.push_back(symTreeNode->left);
				else
					deque.push_front(symTreeNode->left);
				
				#ifdef DEBUG
					std::cout << "left child done!!\n"; 
				#endif
			}
			if (symTreeNode->right)
			{
				#ifdef DEBUG
					std::cout << "right child started!!\n"; 
				#endif
				
				if (dir) 
					deque.push_back(symTreeNode->right);
				else
					deque.push_front(symTreeNode->right);

				#ifdef DEBUG
					std::cout << "right child done!!\n"; 
				#endif
			}
		}


		for (int i = 0; i < steps || steps == -1; i++)
		{

			if (deque.empty()) break;

			SymbolicTreeNode* symTreeNode = deque.front();
			deque.pop_front();
			if (excludedNodes.find(symTreeNode->block) != excludedNodes.end())
				continue;
			if (symTreeNode->isExecuted)
			{
				for (int j = 0; j < 2; j++)
				{
					int k = j;
					if (dir) k = 1 - j;

					SymbolicTreeNode * tempSymTreeNode;
					if (k == 0)
					{
						tempSymTreeNode = symTreeNode->left;
					}
					else
					{
						tempSymTreeNode = symTreeNode->right;
					}
					if (isBFS)
						if (tempSymTreeNode) deque.push_back(tempSymTreeNode);	
					else
						if (tempSymTreeNode) deque.push_front(tempSymTreeNode);
				}
				continue;	
			}
			std::vector<SymbolicTreeNode*> new_blocks = executeBasicBlock(symTreeNode);
			symTreeNode->isExecuted = true;
			BlockStates[symTreeNode->id]=symTreeNode;
			
			Json::Value msg;
			msg["node"] = Json::Value(symTreeNode->id);
			msg["parent"] = Json::Value(symTreeNode->prevId);
			msg["text"] = Json::Value(symTreeNode->state->toString());
			msg["fin"] = Json::Value("0");
			msg["constraints"] = Json::Value(symTreeNode->state->getPathCondition());
			msg["startLine"] = Json::Value(symTreeNode->minLineNumber);
			msg["endLine"] = Json::Value(symTreeNode->maxLineNumber);
			
			toSend["nodes"][currObject++] = msg;
			
			for (int j = 0; j < new_blocks.size(); j++)
			{
				int k = j;
				if (dir) k = new_blocks.size() - 1 - j;

				SymbolicTreeNode * tempSymTreeNode;
				if (k == 0)
				{

					symTreeNode->left = new_blocks[k];
					tempSymTreeNode = symTreeNode->left;
				}
				else
				{
					symTreeNode->right = new_blocks[k];
					tempSymTreeNode = symTreeNode->right;
				}
				if (isBFS)
					deque.push_back(tempSymTreeNode);	
				else
					deque.push_front(tempSymTreeNode);
			}
			#ifdef DEBUG
				std::cout << "size of deque : " << deque.size() << "\n";
			#endif
		}
		#ifdef CIN_SERVER
			std::cout << "proceed? \n";
			std::cout << "prevId : ";
			std::cin >> prevId;
			std::cout << "isBFS : ";
			std::cin >> isBFS;
			std::cout << "steps : ";
			std::cin >> steps;
			std::cout << "dir : ";
			std::cin >> dir; 
			continue;
		#endif

		sendMessageAndSleep(toSend);
	}
}


/**
	Executes all the possible paths in the given function and returns the programState at the end of every path
*/

void SymbolicExecutor::executeFunction(llvm::Function* function)
{
	minLineNumber = std::numeric_limits<unsigned int>::max();
	maxLineNumber = 0;

	for (llvm::Function::iterator b = function->begin(), be = function->end(); b != be; ++b)
	{
		for (auto instruction = b->begin(), e = b->end(); instruction != e; ++instruction)
		{
			blocksArr.push_back(b);
			if (llvm::MDNode *N = instruction->getMetadata("dbg")) 
			{  
				llvm::DILocation Loc(N);
				unsigned Line = Loc.getLineNumber();
				minLineNumber = std::min(minLineNumber,Line);
				maxLineNumber = std::max(maxLineNumber,Line);
			}
		}
	}

	auto rootState = new ProgramState(function->args());
	auto rootBlock = &function->getEntryBlock();
	rootNode = new SymbolicTreeNode(rootBlock, rootState, -1);	
	symbolicExecute();
	
	Json::Value msg;
	msg["fin"] = Json::Value("1");
	Json::FastWriter fastWriter;
	std::string output = fastWriter.write(msg);

	std::cout << "sending this: " << output << std::endl;
	(*socket) << output;
	std::cout << "Function executed"	<< std::endl;
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
		Buffer->release();
	}
	(**mainModuleOrError).materializeAllPermanently();
	return *mainModuleOrError;
}

void SymbolicExecutor::proceed(bool isbfs, int stps, int d, int prev)
{
	isBFS = isbfs;
	steps = stps;
	dir = d;
	prevId = prev; 
	
	std::cout << "WAKE UP!!!!!" << std::endl;
	std::unique_lock<std::mutex> lck(mtx);
	cv.notify_all();
	std::cout << "AWAKEN UP!!!!!" << std::endl;
}


void SymbolicExecutor::execute(bool isbfs, int stps, int d, int prev)
{
	isBFS = isbfs;
	steps = stps;
	dir = d;
	prevId = prev; 

	std::cout << filename.c_str() << " \n";
	llvm::Module* module = loadCode(filename.c_str());
	std::cout << filename.c_str() << " \n";

	auto function = module->getFunction("_Z7notmainii");
	executeFunction(function);
}

void SymbolicExecutor::exclude(std::string inp, bool isNode)
{ 
	int input = stoi(inp);
	unsigned int minLine = input;
	unsigned int maxLine = input;
	if (!isNode)
	{
		auto b = blocksArr[input-minLineNumber];
		for (auto instruction = b->begin(), e = b->end(); instruction != e; ++instruction)
		{
			if (llvm::MDNode *N = instruction->getMetadata("dbg")) 
			{  
				llvm::DILocation Loc(N);
				unsigned Line = Loc.getLineNumber();
				minLine = std::min(minLine,Line);
				maxLine = std::max(maxLine,Line);
			}
		}

		excludedNodes[b] = true;

		Json::Value msg;
		msg["fileId"] = Json::Value(filename.c_str());
		msg["type"] = Json::Value(MSG_TYPE_EXCLUDENODE);
		msg["minLine"] = Json::Value(minLine);
		msg["maxLine"] = Json::Value(maxLine);
		Json::FastWriter fastWriter;
		std::string output = fastWriter.write(msg);
		std::cout << "sending this: " << output << std::endl;
		if (socket)
			(*socket) << output;

		std::cout << "going to sleep" << std::endl;
		std::unique_lock<std::mutex> lck(mtx);
		cv.wait(lck);
		lck.unlock();
		std::cout << "wakeup!! " << std::endl;
	}
	else
		excludedNodes[BlockStates[input]->block] = true;

}

void SymbolicExecutor::sendMessageAndSleep(Json::Value toSend)
{
	Json::FastWriter fastWriter;
	std::string output = fastWriter.write(toSend);
	std::cout << "sending this: " << output << std::endl;
	if (socket)
		(*socket) << output;

	std::cout << "going to sleep" << std::endl;
	std::unique_lock<std::mutex> lck(mtx);
	cv.wait(lck);
	lck.unlock();
	std::cout << "wakeup!! " << std::endl;
}