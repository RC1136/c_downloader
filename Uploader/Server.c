//#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Fileapi.h>
#include <stdio.h>
#include <conio.h>


//#include <stdlib.h>


//#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib,"WS2_32")		//error LNK2019 майже у всіх функціях


#define DEF_BUFFLEN 4096
#define DEF_PORT 27015
#define VERSION "0.5.0.0"



char* getmyname(const char* path)
{
	int end = 0;
	while (path[++end] != '\0');
	int beg = end;
	while (path[--beg] != '\\');
	beg++;
	char* name = (char*)malloc((end - beg + 1) * sizeof(char));
	if (name == NULL)
		return getmyname(path);

	for (int i = 0; i < end - beg; i++)
		name[i] = path[beg + i];
	name[end - beg] = '\0';
	return name;
}



SOCKET GetListener(const char* addr, const int port)
{
	int res;
	char* strport = (char*)malloc(6 * sizeof(char));
	if (strport == NULL)
		return GetListener(addr, port);
	_itoa((int)port, strport, 10);


	WSADATA wsaData;
	SOCKET ListenSocket = INVALID_SOCKET;
	struct addrinfo* result = NULL;
	struct addrinfo hints;

	//Ініціалізую сокет
	res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != 0) {
		printf("WSAStartup failed (%d)\n", res);
		return INVALID_SOCKET;
	}

	ZeroMemory(&hints, sizeof(hints));
	//memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//Отримуєм адресу серверу і порт
	res = getaddrinfo(	//https://docs.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo
		NULL,				//Адреса
		strport,			//Порт
		&hints,				//Рекомендації по сокету (що ми хочем)
		&result				//Інформація по сокету (що ми отримали)
	);
	if (res != 0) {
		printf("getaddrinfo failed (%d)\n", res);
		WSACleanup();
		return INVALID_SOCKET;
	}

	//Створюю сокет для з'єднання з сервером
	ListenSocket = socket(//https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
		result->ai_family,
		result->ai_socktype,
		result->ai_protocol
	);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed (%d)\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return INVALID_SOCKET;
	}

	//Встановлюю прослуховуючий (Listening) сокет
	res = bind(//https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-bind
		ListenSocket,			//Дескриптор сокета
		result->ai_addr,		//Адреса (і всяке таке)
		(int)result->ai_addrlen	//Довжина, в байтах, значення імені (костиль - інфа сотка)
	);
	if (res == SOCKET_ERROR) {
		printf("bind failed (%d)\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}
	freeaddrinfo(result);
	free(strport);

	return ListenSocket;
}


SOCKET AcceptClient(SOCKET ListenSocket)
{
	printf("Waiting for client...\n");
	int res;
	SOCKET ClientSocket = INVALID_SOCKET;
	res = listen(//https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-listen
		ListenSocket,	//Сокет
		SOMAXCONN		//Довжина черги
	);
	if (res == SOCKET_ERROR) {
		printf("listen failed (%d)\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}
	struct sockaddr_in client_addrinfo;
	ZeroMemory(&client_addrinfo, sizeof(client_addrinfo));
	int tmp = sizeof(client_addrinfo);

	//Приймаю з'єднання
	ClientSocket = accept(//https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-accept
		ListenSocket,
		(struct sockaddr*)&client_addrinfo,
		&tmp
	);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed (%d)\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return INVALID_SOCKET;
	}

	//getpeername(ClientSocket, (struct sockaddr*)&client_addrinfo, &tmp);

	char cliIP[INET_ADDRSTRLEN];
	inet_ntop(client_addrinfo.sin_family, &(client_addrinfo.sin_addr), cliIP, INET_ADDRSTRLEN);
	printf("Cient's ip is: %s\n", cliIP); //inet_ntoa(client_addrinfo.sin_addr)
	


	return ClientSocket;
}

BOOL SendFile(SOCKET ClientSocket, const char* file_name, const int file_size, char* buf)
{
	int res, bytes_read, bytes_sent;
	//char* buf = (char*)malloc(DEF_BUFFLEN * sizeof(char));
	if (buf == NULL)
		return FALSE;

	HANDLE File = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (File == HFILE_ERROR) {
		printf("Failed to open file %s (%d)\n", file_name, GetLastError());
		return FALSE;
	}


	//5
	bytes_sent = send(												//Відправляю назву файлу
		ClientSocket,	//Сок
		file_name,
		strlen(file_name)+1,
		0
	);
	//8
	bytes_read = recv(ClientSocket, buf, DEF_BUFFLEN, 0);		//Отримую відповідь (к-сть отриманих байтів)

	if (atoi(buf) == bytes_sent) {									//Звіряю з тим, що відправляв
		printf("File name delivered\n");
	}
	else {
		printf("File name send error (%s)\n", buf);
	}


	//відправляю розмір файлу
	_itoa(file_size, buf, 10);
	//9
	bytes_sent = send(ClientSocket, buf, strlen(buf) + 1, 0);
	//12
	bytes_read = recv(ClientSocket, buf, DEF_BUFFLEN, 0);
	if (atoi(buf) == bytes_sent) {
		printf("File size delivered\n");
	}
	else {
		printf("File size send error (%d)\n", WSAGetLastError());
	}


	int sent_all = 0;
	printf("Bytes sent:           ");
	//13
	do {
		res = ReadFile(											//Записую дані з файлу в буфер
			File,
			buf,
			DEF_BUFFLEN,
			&bytes_read,
			NULL
		);
		if (!res) {
			printf("Readfile failed (%d)", GetLastError());
			return FALSE;
		}



		bytes_sent = send(					//Відправляю буфер
			ClientSocket,	//Сок
			buf,
			bytes_read,
			0
		);
		if (bytes_sent == -1) {
			printf("send failed (%d)\n", WSAGetLastError());
			return FALSE;
		}
		//printf("Sent %d bytes\n", bytes_sent);
		sent_all += bytes_sent;
		printf("\b\b\b\b\b\b\b\b\b\b% 10d", sent_all);

	} while (bytes_read == DEF_BUFFLEN);
	putchar('\n');

	bytes_read = recv(ClientSocket, buf, DEF_BUFFLEN, 0);
	if (atoi(buf) == file_size) {
		printf("File delivered\n");
	}
	else {
		printf("File delivered incorrectly (%s)\n", buf);
	}
	CloseHandle(File);
	//free(buf);
	return TRUE;
}



//
BOOL SendFolder(SOCKET ClientSocket, const char* path, char* buf)
{
	int res;

	char* current_directory = (char*)malloc(MAX_PATH * sizeof(char));
	if (current_directory == NULL) {
		return FALSE;
	}
	GetCurrentDirectoryA(MAX_PATH, current_directory);
	res = SetCurrentDirectoryA(path);
	if (!res) {
		printf("Failed to set current directory as \"%s\"\n", path);
		return FALSE;
	}
	int bytes_read, bytes_sent;
	//char* buf = malloc(DEF_BUFFLEN * sizeof(char));					//буфер, через який я буду пересилати повідомлення
	if (buf == NULL)
		return FALSE;
	GetCurrentDirectoryA(MAX_PATH, buf);
	printf("Current directory is \"%s\"\n", buf);					//Виводжу назву директорії
	char* folder_name = getmyname(buf);

	//1
	//Віправляю назву папки
	bytes_sent = send(ClientSocket, ".\\", 2, 0);
	bytes_sent += send(ClientSocket, folder_name, strlen(folder_name) + 1, 0);

	//4
	//Перевіряю, чи дійшло
	bytes_read = recv(ClientSocket, buf, DEF_BUFFLEN, 0);
	if(atoi(buf) == bytes_sent){
		printf("Folder name delivered\n");
	}
	else {
		printf("Folder name send error(%d)\n", WSAGetLastError());
		return FALSE;
	}


	WIN32_FIND_DATAA ffd;
	HANDLE flister;
	flister = FindFirstFileA(".\\*", (LPWIN32_FIND_DATAA)&ffd);						//Шукаю перший файл
	do {
		printf("Processing file \"%s\"\n", ffd.cFileName);
		if (!strcmp(ffd.cFileName, ".") || !strcmp(ffd.cFileName, "..")){
			continue;
		}
		//Якщо файл - папка, то надсилаю папку
		if (ffd.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {
			char* tmp_path = (char*)malloc(MAX_PATH * sizeof(char));
			if (tmp_path == NULL)
				return FALSE;
			strcpy_s(tmp_path, 3, ".\\");
			strcpy_s(&tmp_path[2], MAX_PATH, ffd.cFileName);

			SendFolder(ClientSocket, tmp_path, buf);

			free(tmp_path);
			continue;
		}


		printf("Sending file \"%s\"\n", ffd.cFileName);
		
		//5
		res = SendFile(ClientSocket, ffd.cFileName, ffd.nFileSizeLow, buf);
		if (!res) {
			return FALSE;
		}


	} while (FindNextFileA(flister, (LPWIN32_FIND_DATAA)&ffd) != 0);
	bytes_sent = send(ClientSocket, "\0", 1, 0);
	bytes_read = recv(ClientSocket, buf, DEF_BUFFLEN, 0);




	printf("All files delivered!\n");
	SetCurrentDirectoryA(current_directory);
	free(current_directory);
	//free(buf);
	FindClose(flister);

	

	return TRUE;
}

DWORD WINAPI SendThread(LPVOID param)
{
	SOCKET ClientSocket = *(SOCKET*)&param;
	char* path = *(char**)(&param + sizeof(SOCKET));
	char* buf = *(char**)(&param + sizeof(SOCKET) + sizeof(char*));

	LPOFSTRUCT tmp = malloc(sizeof(OFSTRUCT));
	OpenFile(path, tmp, OF_EXIST);
	


}

DWORD WINAPI RecvOOBThread(LPVOID param)
{
	DWORD code;
	switch (recv(*(SOCKET*)&param, &code, sizeof(DWORD), MSG_OOB)) {
	case 4:
		return code;
	case 0:
		return 0;
	case -1:
		return WSAGetLastError();
	}
}

DWORD WINAPI ServerThread(LPVOID param)
{

	char* buf = (char*)malloc(DEF_BUFFLEN*sizeof(char));
	SOCKET ClientSocket = *(SOCKET*)&param;
	*(char**)(&param + sizeof(SOCKET) + sizeof(char*)) = buf;
	HANDLE Sender = CreateThread(NULL, 0, SendThread, param, 0, NULL);
	HANDLE OOBthread = CreateThread(NULL, 64, RecvOOBThread, &ClientSocket, 0, NULL);

	WaitForSingleObject(OOBthread, INFINITE);
	DWORD code;
	GetExitCodeThread(OOBthread, &code);


	//SendFolder(ClientSocket, folder_name,);
	closesocket(ClientSocket);
	free(buf);

	return 0;
}


//https://docs.microsoft.com/en-us/windows/win32/winsock/complete-server-code
int main(int argc, char* argv[])
{
	
	short port = DEF_PORT;
	//const char* _myname = getmyname(argv[0]);

	char* folder_name = malloc((MAX_PATH + 1) * sizeof(char));
	if (folder_name == NULL) {
		return 1;
	}
	if (argc > 1) {
		int tmp = strcpy_s(folder_name, MAX_PATH, argv[1]);
	}
	else {
		strcpy_s(folder_name, MAX_PATH, ".\\FROM");
	}

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;
	ListenSocket = GetListener("", (int)port);



	HANDLE client_processes[2048];
	int number = 0;
	//Починаю відправляти файлики
	while (1) {
		//Чекаю з'єднання
		ClientSocket = AcceptClient(ListenSocket);
		DWORD_PTR param = malloc(sizeof(SOCKET) + sizeof(char*));
		*(SOCKET*)&param = ClientSocket;
		*(char*)(&param + sizeof(SOCKET)) = folder_name;

		//client_threads[number] = CreateThread(NULL, 0, ServerThread, param, 0, NULL);
		//SetThreadPriority(client_threads[number], THREAD_PRIORITY_ABOVE_NORMAL);
		CreateProcessA("Uploader.exe", , NULL, NULL, TRUE, , );

		free(param);
	}
	WaitForMultipleObjects(number, client_threads, TRUE, INFINITE);
	free(folder_name);
	WSACleanup();



	/**/
	closesocket(ListenSocket);
	while (getchar());
	return 0;




	return 0;




}