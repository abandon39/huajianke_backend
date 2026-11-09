#include <string>
#include <thread>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#endif

#include <pans/utils/thread_utils.h>

int main()
{
	std::cout << "1: main thread id: " << pans::GetThreadId() << std::endl;
	std::cout << "2: main thread name: " << pans::GetThreadName() << std::endl;
	sleep(30); // 等30s，给人足够的时间用top看进程id等有关内容（top -H -p <pid>）
	pans::SetThreadName("main_thread");
	std::cout << "3: main thread name: " << pans::GetThreadName() << std::endl;
	sleep(30); // 等10s，
	pans::SetThreadName("花间客真帅");
	std::cout << "4: main thread name: " << pans::GetThreadName() << std::endl;
	sleep(30);
	pans::SetThreadName("磐石磐石坚如磐石"); // 虽然只有15字节，但是预期应该能输出完整的内容，但是top只能看前5个字节。
	std::cout << "5: main thread name: " << pans::GetThreadName() << std::endl;
	sleep(30);
	std::thread worker([](){
		std::cout << "6: son thread id: " << pans::GetThreadId() << std::endl;
		std::cout << "7: son thread name: " << pans::GetThreadName() << std::endl;
		sleep(30);
		pans::SetThreadName("son-thread-long-name");
		std::cout << "8: son thread name: " << pans::GetThreadName() << std::endl;
		sleep(30);
		pans::SetThreadName("");
		std::cout << "9: son thread name: " << pans::GetThreadName() << std::endl;
	});
	worker.join(); // 等待worker退出
	return 0;
}