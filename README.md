# spmc-queue
lock free spmc queue using a ring buffer implementation 

desgin is based around version numbers, even means a node is empty or already consumed, odd when the node contains data ready to be read 
this design does not wait for slow consumers, the producer will overwrite a node even if the slow consumer has not yet read it 

![image](https://github.com/user-attachments/assets/c04c80e7-51dd-4325-866a-5733acdbe0ad)

ref: https://www.youtube.com/watch?v=8uAW5FQtcvE
