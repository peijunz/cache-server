# Project README file

# Part 2: Shared Memory
## Design
+ Obviously the client 
+ Use socket to communicate between proxy and cache to get transfer command and shared memory info. Shared memory is created by proxy, and the key of shared memory is then sent to cache server. After completion 
+ The synchronization between cache's writing to memory and proxy's reading is by mutex
+ Neither the cache daemon nor the proxy should crash if the other process is not started already. 客户端需要等待服务器启动？或者用message quene?

+ 每个proxy的线程有自己的消息类型用于发送文件请求以及获取回复，对应一个msg type是自己的线程id。发送的消息是0型，但是包含了自己的类型信息
+ 缓存服务器线程接受0型信息后，读取发送线程给的共享内存信息，并且发送对应类型的信息，被对应线程读取从而交流信息。

+ msgtype is used to distinguish message of different purposess
+ msg quene has thread safety

+ 每个proxy线程分配一段共享内存
+ semaphore或者msg quene同步 

先实现一个简单的发送接收消息的功能


file size == -1 means not found
---

This is **YOUR** Readme file.

## Project Description
We will manually review your file looking for:

- A summary description of your project design.  If you wish to use grapics, please simply use a URL to point to a JPG or PNG file that we can review

- Any additional observations that you have about what you've done. Examples:
	- __What created problems for you?__
	- __What tests would you have added to the test suite?__
	- __If you were going to do this project again, how would you improve it?__
	- __If you didn't think something was clear in the documentation, what would you write instead?__

## Known Bugs/Issues/Limitations

__Please tell us what you know doesn't work in this submission__

## References

__Please include references to any external materials that you used in your project__

