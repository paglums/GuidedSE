# User-guided Symbolic Execution with Visualization
A novel framework for user-guided symbolic execution with effective visualization. The visualization enables the user to control how a particular program is symbolically executed during the symbolic analysis. Based on where the symbolic executor is headed, the user can choose a search strategy, prune or focus on certain areas of code, and create models to avoid costly or impossible to cover functions. In addition to controlling the state-space explosion problem inherent in symbolic execution with the help of user-guidance, the visualization can make symbolic-execution more accessible to the general developer/tester community. 

#### Here's a demo of the tool
[![IMAGE ALT TEXT HERE](https://github.com/paglums/GuidedSE/blob/master/images/video_image.jpg)](https://www.youtube.com/watch?v=0VNe4BjjF90)


Our user guided features allow the user to begin symbolic execution from any node in the tree or exclude certain areas from the execution. In case of an external function call, users can provide a model for the function. In the viualization, a node in the execution tree represents the longest consecutive sequence of statements that can be executed without encountering a conditional branch execution. The text below each node shows the last constraint that was added on that path. Further information about the program state at each node can be viewed by the user on hovering the mouse at the desired node. The tooltip shows what the values of the different variables would be (in terms of the function input) if the execution followed the highlighted path.



A screenshot of the complete window can be seen in the figure below:


![Screenshot of the tool](https://github.com/paglums/GuidedSE/blob/master/images/README_screenshot.PNG)
