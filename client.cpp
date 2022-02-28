#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <sys/wait.h>

#define BUFFER_SIZE 2048
#define IP "127.0.0.1"
#define PORT 6666

using std::string;

int serverSocket = 0;
bool quit = false, logged = false;
char ID[8];

pthread_t sendThread, receiveThread;

void catch_CtrlC(int sig) { quit = true; }

void* SendHandler (void* arg);
void* ReceiveHandler(void* arg);

int Check (int result, const char* errorMessage);
bool KeyWord (char* buffer);
void SetConsoleTitle(string title);

int main(){

	system("clear");

	struct sockaddr_in serverAddr;

	/* Socket settings */
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(IP);
	serverAddr.sin_port = htons(PORT);

	// Connect to Server
	Check (connect(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)), "[!]Connection failed.");

	while (logged == false) {

		bzero(ID, 8);
		printf("Please enter your ID: ");
		fgets(ID, 8, stdin);

		send(serverSocket, ID, strlen(ID), 0);

		bzero(ID, 8);

		int size = recv(serverSocket, ID, 8, 0); ID[size] = '\0';

		system("clear");

		if (strncmp(ID, "OK", 2) == 0) logged = true;
		else printf("\033[31m[!]Incorrect ID.\033[0m\n");
	}

	signal(SIGINT, catch_CtrlC);
	printf("\033[32mWelcome to the QuizzGame chat room! \033[0m \n\n");
	SetConsoleTitle("\033]0; Client \007");

	Check (pthread_create(&sendThread, NULL, &SendHandler, NULL), "\033[31m[!]\033[0mSend Thread failed");

 	Check( pthread_create(&receiveThread, NULL, &ReceiveHandler, NULL), "\033[31m[!]\033[0mReceive Thread failed");

	while (true){
		if (quit == true) {
			write(serverSocket, "!exit", 5);

			system("clear");
			printf("\n\033[31m[!]\033[0mYou left the server.\n\n");
			break;
    	}
	}

	close(serverSocket);

	return 0;
}

int Check (int result, const char* errorMessage) {
    if (result < 0) {
        perror(errorMessage);
        exit(1);
    }

    return result;
}

bool KeyWord (char* buffer) {

	if (strncmp(buffer, "Exam Name: ", 11) == 0) return true;
	if (strncmp(buffer, "Question: ", 10) == 0) return true;
	if (strncmp(buffer, "Option a: ", 10) == 0) return true;
	if (strncmp(buffer, "Option b: ", 10) == 0) return true;
	if (strncmp(buffer, "Option c: ", 10) == 0) return true;
	if (strncmp(buffer, "Option d: ", 10) == 0) return true;
	if (strncmp(buffer, "Index of correct answers: ", 26) == 0) return true;
	if (strncmp(buffer, "Question Number: ", 17) == 0) return true;

	return false;
}

void SetConsoleTitle(string title) { write(1, title.c_str(), title.size()); }
  
char* Timer (char* question) {
    
    int p[2]; pipe(p);
    int pid = fork();
    char* buffer = new char[32];
	strcpy(buffer, "empty\n");
	write(p[1], buffer, strlen(buffer));

    if (pid == 0) {

        close(p[0]);
        bool ok = false;

        while (true) {
            system("clear");
            if (ok) printf("\033[1mRaspuns inregistrat:\033[0m %s\n", buffer);
            printf("%s\n> ", question);
			fflush(stdout);

			bzero(buffer, 32);
            fgets(buffer, 32, stdin);
			
			if (buffer[0] != '\n') {
				write(p[1], buffer, strlen(buffer));
				ok = true;
			}
        }
    } else {
        int index = 5;
        close(p[1]);

        while (index >= 0) {
            SetConsoleTitle("\033]0; " + std::to_string(index) + " sec left\007");
            sleep(1);
            index--;
        }

        kill(pid, 1);
        wait(NULL);

        int size = read(p[0], buffer, 32); buffer[size - 1] = '\0'; close(p[0]);
        index = 0;

        while(index < strlen(buffer)) {
            if (buffer[index] == '\n') {
                strcpy(buffer, buffer + index + 1);
                index = 0;
            }
            index++;
        }

        return buffer;
    }
}

void* ReceiveHandler(void* arg) {
	char buffer[BUFFER_SIZE];
	
	while (true) {
		bzero(buffer, BUFFER_SIZE);
		int receive = recv(serverSocket, buffer, BUFFER_SIZE, 0);

		if (strcmp(buffer, "Start") == 0) {

			send(serverSocket, "Start", 6, 0);

			pthread_cancel(sendThread);

			while (true) {
				bzero(buffer, BUFFER_SIZE);
				int receive = recv(serverSocket, buffer, BUFFER_SIZE, 0);

				if (strcmp(buffer, "End") == 0) {
					break;
				}
				char answer[8] = {};
				strcpy(answer, Timer(buffer));
				write(serverSocket, answer, strlen(answer));
			}

			system("clear");
			printf("\033[32mWelcome to the QuizzGame chat room! \033[0m \n\n");
			SetConsoleTitle("\033]0; Client \007");
			pthread_create(&sendThread, NULL, &SendHandler, NULL);

		}
		else if (receive > 0) {
			if (KeyWord(buffer) == true) {
				printf("\x1b[2K\r%s", buffer);
				fflush(stdout);
			}
			else {
				printf("\r\x1b[2K%s\n\033[1;33mMe\033[0m: ", buffer);
				fflush(stdout);
			}
  			
		} else if (receive == 0) {
			break;
		}
	}
	
	bzero(buffer, BUFFER_SIZE);

  return NULL;
}

void* SendHandler (void* arg) {
	
	char buffer[BUFFER_SIZE];

	while(true) {
	
		bzero(buffer, BUFFER_SIZE);

		printf("\033[1;33mMe\033[0m: ");
  		fflush(stdout);
		fgets(buffer, BUFFER_SIZE, stdin);

		if (strncmp(buffer, "!exit", 5) == 0) {
			send(serverSocket, buffer, strlen(buffer), 0);
			break;
		} else {
			send(serverSocket, buffer, strlen(buffer), 0);
		}

		bzero(buffer, BUFFER_SIZE);
	}

	catch_CtrlC(2);
	return NULL;
}