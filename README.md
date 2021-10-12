# Project 3: Semaphores & Message Passing

CS 4760: Operating Systems  
Lexi Anderson  

## Overview
Project 3 once again tackles the license manager exercise from Project 2. This time, 
I used a semaphore to control access to the license object. I was able to utilize a lot
 of the code I wrote for Project 2, either by copying segments of code directly or
 building off of them.


## Difficulties
Since this project was based so heavily on our previous project, which has not yet 
been graded, I could not use the feedback from that project to improve this one. There 
may be mistakes that I made in Project 2 that remained in my implementation of Project 
3. I did my best to thoroughly test each aspect of my code.


## Semaphores
I was a bit unsure of how to implement semaphores in this project, so I settled on 
using a semaphore to protect the sections of code where the license object functions 
were called. I did not want more than one process to access the license object at the 
same time.
