# spmc-queue
lock free spmc queue using a ring buffer implementation 

desgin is based around version numbers, even means a node is empty or already consumed, odd when the node contains data ready to be read 
this design does not wait for slow consumers, the producer will overwrite a node even if the slow consumer has not yet read it 

![image](https://github.com/user-attachments/assets/297fc301-f1c0-4410-81d9-e2cccc61033c)


ref: https://www.youtube.com/watch?v=8uAW5FQtcvE
