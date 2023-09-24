#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#define NumberCons 7
#define Port 10243
#define Bufer 255

// Объявление обработчика сигналов
volatile sig_atomic_t wasSigHup = 0;
void sigHupHandler(int r) {
	wasSigHup = 1;
}

int main() {
	int _socket = 0;
	_socket = socket(AF_INET, SOCK_STREAM, 0); // Создание сокета

	// Указываем адрес и порт для привязки соккета
	struct sockaddr_in _sockaddr;	
	memset(&_sockaddr, 0, sizeof(_sockaddr));
	_sockaddr.sin_family = AF_INET;
	_sockaddr.sin_port = htons(Port);	
	_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	listen(_socket, NumberCons);
	printf("Server listen\n Port: ", Port);

	// Регистрация обработчика сигнала
	struct sigaction sa;
	sigaction(SIGHUP, NULL, &sa);	// Заливаем нужные структуры
	sa.sa_handler = sigHupHandler;	// Модифицируем
	sa.sa_flags |= SA_RESTART;	// Устанавливаем флаги
	sigaction(SIGHUP, &sa, NULL);	// Заливаем в ядро

	// Блокировка сигнала
	sigset_t origMask;
	sigset_t blockedMask;
	sigemptyset(&blockedMask);
	sigaddset(&blockedMask, SIGHUP);
	sigprocmask(SIG_BLOCK, &blockedMask, &origMask);


	int clients[NumberCons] = { 0 };
	int clients_num = 0;

	char data[Bufer] = { 0 };

	while (1) {
		// Работа основного цикла
		int maxFd = _socket;
		fd_set fds;
		// Подготовка спсика файловых дескрипторов
		FD_ZERO(&fds);
		FD_SET(_socket, &fds); // Серверный файловый дескриптор
		for (int i = 0; i < clients_num; i++) {
			FD_SET(clients[i], &fds);
			if (clients[i] > maxFd) {
				maxFd = clients[i];
			}
		}

		if (pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask) == -1){
			if (errno == EINTR) {
				printf("pselect ERROR");
				return 1;
			}
		}
		if (wasSigHup == 1) {
			printf("Stoping\n");
			wasSigHup = 0;
			break;
		}

		if (FD_ISSET(_socket, &fds)) {
			int connection = accept(_socket, NULL, NULL);
			if (clients_num + 1 > NumberCons) {
				printf("Too many clients\n");
				close(connection);
				continue;
			}
			clients[clients_num] = connection;
			clients_num++;
			printf("Successful connection: %d\n", connection);
		}

		for (int i = 0; i < clients_num; i++) {
			if (FD_ISSET(clients[i], &fds)) {
				memset(&data, 0, Bufer);
				ssize_t n = read(clients[i], &data, Bufer - 1);
				if (n > 0) {
					n--;
				}
				if (n <= 0 || !strcmp("quit", data)) {
					close(clients[i]);
					printf("Closed connection! Client %d\n", clients[i]);
					clients[i] = clients[clients_num - 1];
					clients[clients_num - 1] = 0;
					clients_num--;
					i--;
					continue;
				}
				printf("Client %d sent %ld bytes: %s\n", clients[i], n, data);
			}
		}
	}
	close(_socket);
}
