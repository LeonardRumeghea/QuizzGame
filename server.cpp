#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <list>
#include <string>
#include <sqlite3.h>
#include <iostream>
#include <ctime>
#include "User.h"

#define BUFFER_SIZE 2048
#define IP "127.0.0.1"
#define PORT 6666

using std::string;
using std::list;
using std::cout;
using std::endl;

list<User*> users;
sqlite3* DB;

pthread_mutex_t userMutex = PTHREAD_MUTEX_INITIALIZER;

/* Main Functions */
bool ChatRoom(User* user);
bool ExamRoom(User* user);
void* HandleClient (void *arg);

/* User Management Functions */
void AddUser (User *user);
void RemoveUser (string ID);
void SendMessage (char *msg, char* ID);

/* Queries */
int GetQuestionsCount(string tableName);
char* GetQuestion(string examName, int count);
char* GetQuestionAnswer(int questionNumber, string examName);
bool AddResult(User* user, float points);
char* ShowResults(User* user);
bool CreateExam(string examName);
bool DropExam(string examName);
bool InsertQuestion (string examName, string question, string option1, string option2, string option3, string option4, string correctAnswer);
bool UpdateQuestion (string examName, string questionNumber, string question, string option1, string option2, string option3, string option4, string correctAnswer);
bool RemoveQuestion (string examName, string questionNumber);
bool ShowQuestions (string examName, int socket);
bool ShowExams (int socket);
bool AddUser(string firstName, string LastName, string Grad);

/* Check Functions */
int Check (int value, const char* errorMessage);
bool CheckID(string ID, User* user);
float CheckAnswer(char* answer, int questionNumber, string examName);

/* Utility Functions */
void SetConsoleTitle(string title) { write(1, title.c_str(), title.size()); }
void CreateUsersDB();
bool TryCommands(int receiveLength, char* buffer, User* user);

int main(){ 

    system("clear");

    int serverSocket = 0, userSocket = 0;  
    struct sockaddr_in serverAddr;
    struct sockaddr_in userAddr;

    /* Socket settings */
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(IP);
    serverAddr.sin_port = htons(PORT);

    /* Bind */
    Check(bind(serverSocket, (struct sockaddr*)& serverAddr, sizeof(serverAddr)), "\033[31m[!]\033[0mSocket binding failed");

    /* Listen */
    Check(listen(serverSocket, 10), "\033[31m[!]\033[0mSocket listening failed");

    sqlite3_open("QuizzGame.db", &DB);

    //CreateUsersDB();

    SetConsoleTitle("\033]0; Server \007");
	printf("\033[32m[+]\033[0mThe server was successfully launched!\n");

	while(true){

		socklen_t size = sizeof(userAddr);
		userSocket = accept(serverSocket, (struct sockaddr*)&userAddr, &size);

        User* user = new User[1];
		user->address = userAddr;
		user->socket = userSocket;

		Check(pthread_create(&user->thread, NULL, &HandleClient, (void*)user), "\033[31m[!]\033[0mPthread_create failed");

		sleep(1);
	}

    close(serverSocket);

	return 0;
}


/* Main Functions */
void* HandleClient (void *arg) {

	User *user = (User*) arg;
    char buffer[BUFFER_SIZE], cache[BUFFER_SIZE + 128];
    int receiveLength = 0;
    bool run = true;

    while (user->logged == false) {

        bzero(buffer, BUFFER_SIZE);
        receiveLength = recv(user->socket, buffer, BUFFER_SIZE, 0);
        Check(receiveLength, "[!]No ID received");
        buffer[receiveLength - 1] = '\0';
	
        if (receiveLength == 0){
            close(user->socket);
            RemoveUser(user->ID);
            delete user;
            pthread_detach(pthread_self());
            //system("clear");

            return NULL;
        }

        if (CheckID(buffer, user)) {
            if (user->logged == true) {

                write(user->socket, "OK", 2);

                sprintf(buffer, "\033[32m[+]\033[0m%s %s has joined", user->lastName, user->firstName);
                printf("%s\n", buffer);
                SendMessage(buffer, user->ID);

            } else write(user->socket, "NOT OK", 6);
        } else {
            write(user->socket, "NOT OK", 6);
        }
    }

    AddUser(user);

    while (run == true) {

        run = ChatRoom(user);
        if (user->examRoom) run = ExamRoom(user);
    }

    close(user->socket);
    RemoveUser(user->ID);
    delete user;
    pthread_detach(pthread_self());

	return NULL;
}

bool ChatRoom(User* user) {

    char buffer[BUFFER_SIZE], cache[BUFFER_SIZE + 128];
    int receiveLength = 0;
	bool chatMode = true;

    while(chatMode == true){

        bzero(buffer, BUFFER_SIZE);
		receiveLength =  recv(user->socket, buffer, BUFFER_SIZE, 0);
        buffer[receiveLength - 1] = '\0';

        if (receiveLength == 0 || strcmp(buffer, "!exit") == 0){
			
            sprintf(buffer, "\033[31m[-]\033[0m%s %s has left", user->lastName, user->firstName);
		    printf("%s\n", buffer);
			
            SendMessage(buffer, user->ID);
			chatMode = false;
            return false;
        }

        if (user->examRoom == true and strcmp(buffer, "Start") == 0 ) return true;

        if (buffer[0] == '!') {
            		
            if (strncmp(buffer, "!start ", 7) == 0) {
                if (user->IsProfesor() == true) {
                    pthread_mutex_lock(&userMutex);
                    for (auto it : users) {
                        if (strcmp(it->grad, "Student") == 0 && it->examRoom == false) {
                            strcpy(it->examName, buffer + 7);
                            sprintf(it->profesorName, "%s %s", user->lastName, user->firstName);
                            it->examRoom = true;
                            send(it->socket, "Start", 5, 0);
                        }
                    }
                    pthread_mutex_unlock(&userMutex);

                } else write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 

            } 
            else if (strncmp(buffer, "!results", 8) == 0) {
                if (user->IsStudent()) {
                    
                    bzero(buffer, BUFFER_SIZE);
                    strcpy(buffer, ShowResults(user));

                    write(user->socket, buffer, strlen(buffer));
                }
                else {
                    write(user->socket, "\033[31m[!]\033[0mAs a teacher, you are unable to use this command!", 61);
                }
            
            } 
            else if (strncmp(buffer, "!create", 7) == 0) {
                if (user->IsProfesor()) {

                    if (CreateExam(buffer + 8) == true) {
                        write(user->socket, "\033[32m[+]\033[0mTable successfully created.", 39);
                    }
                    else {
                        write(user->socket, "\033[31m[!]\033[0mCreate table failed.", 32);
                    }
                } else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!drop", 5) == 0) {
                if (user->IsProfesor()) {

                    if (DropExam(buffer + 6) == true) {
                        write(user->socket, "\033[32m[+]\033[0mTable successfully dropted.", 37);
                    }
                    else {
                        write(user->socket, "\033[31m[!]\033[0mDrop table failed.", 30);
                    }
                } else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!insert", 7) == 0) {
                if (user->IsProfesor()) {
                    char examName[16], question[64], option1[16], option2[16], option3[16], option4[16], correctAnswer[16];
                    int length = 0;

                    write (user->socket, "Exam Name: ", 11);
                    length = read (user->socket, examName, 16); examName[length - 1] = '\0';
                    write (user->socket, "Question: ", 10);
                    length = read (user->socket, question, 16); question[length - 1] = '\0';

                    write (user->socket, "Option a: ", 11);
                    length = read (user->socket, option1, 16); option1[length - 1] = '\0';
                    write (user->socket, "Option b: ", 11);
                    length = read (user->socket, option2, 16); option2[length - 1] = '\0';
                    write (user->socket, "Option c: ", 11);
                    length = read (user->socket, option3, 16); option3[length - 1] = '\0';
                    write (user->socket, "Option d: ", 11);
                    length = read (user->socket, option4, 16); option4[length - 1] = '\0';

                    write (user->socket, "Index of correct answers: ", 26);
                    length = read (user->socket, correctAnswer, 16); correctAnswer[length - 1] = '\0';

                    if (InsertQuestion(examName, question, option1, option2, option3, option4, correctAnswer) == true) {

                        write(user->socket, "\033[32m[+]\033[0mTable successfully inserted.", 40);
                    }
                    else {
                        write(user->socket, "\033[31m[!]\033[0mTable insert failed.", 32);
                    }
                } else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!update", 7) == 0) {
                if (user->IsProfesor() == true) {     
                    char questionNumber[8], examName[16], question[64], option1[16], option2[16], option3[16], option4[16], correctAnswer[16];
                    int length = 0;

                    write (user->socket, "Exam Name: ", 11);
                    length = read (user->socket, examName, 16); examName[length - 1] = '\0';
                    write (user->socket, "Question Number: ", 17);
                    length = read (user->socket, questionNumber, 8); questionNumber[length - 1] = '\0';
                    write (user->socket, "Question: ", 10);
                    length = read (user->socket, question, 16); question[length - 1] = '\0';

                    write (user->socket, "Option a: ", 11);
                    length = read (user->socket, option1, 16); option1[length - 1] = '\0';
                    write (user->socket, "Option b: ", 11);
                    length = read (user->socket, option2, 16); option2[length - 1] = '\0';
                    write (user->socket, "Option c: ", 11);
                    length = read (user->socket, option3, 16); option3[length - 1] = '\0';
                    write (user->socket, "Option d: ", 11);
                    length = read (user->socket, option4, 16); option4[length - 1] = '\0';

                    write (user->socket, "Index of correct answers: ", 26);
                    length = read (user->socket, correctAnswer, 16); correctAnswer[length - 1] = '\0';

                    if (std::atoi(questionNumber) > GetQuestionsCount(examName)) {
                        sprintf(buffer, "\033[31m[!]\033[0mThere is no question number %s!", questionNumber);
                        write(user->socket, buffer, strlen(buffer));
                    }else {
                        if (UpdateQuestion(questionNumber, examName, question, option1, option2, option3, option4, correctAnswer) == true) {

                            write(user->socket, "\033[32m[+]\033[0mTable successfully updated.", 39);
                        }
                        else {
                            write(user->socket, "\033[31m[!]\033[0mTable update failed.", 32);
                        }
                    }
                } else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!remove", 7) == 0) {
                if (user->IsProfesor() == true) {
                    char questionNumber[8], examName[16];
                    int length = 0;

                    write (user->socket, "Exam Name: ", 11);
                    length = read (user->socket, examName, 16); examName[length - 1] = '\0';
                    write (user->socket, "Question Number: ", 17);
                    length = read (user->socket, questionNumber, 8); questionNumber[length - 1] = '\0';


                    if (std::atoi(questionNumber) > GetQuestionsCount(examName)) {
                        sprintf(buffer, "\033[31m[!]\033[0mThere is no question number %s!", questionNumber);
                        write(user->socket, buffer, strlen(buffer));
                    }else {
                        if (RemoveQuestion(examName, questionNumber) == true) {

                            write(user->socket, "\033[32m[+]\033[0mTable row deleted successfully.", 43);
                        }
                        else {
                            write(user->socket, "\033[31m[!]\033[0mTable row delete failed.", 36);
                        }
                    }
                } else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strcmp(buffer, "!show") == 0) { 
                if (user->IsProfesor()) {
                    if (ShowExams(user->socket) == false) {
                        write(user->socket, "\033[31m[!]\033[0mThere was a problem!", 32); 
                    }
                }
                else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!show ", 6) == 0) { 
                if (user->IsProfesor()) {
                    if (ShowQuestions(buffer + 6, user->socket) == false) {
                        write(user->socket, "\033[31m[!]\033[0mThere was a problem displaying the questions!", 57); 
                    }
                }
                else {
                    write(user->socket, "\033[31m[!]\033[0mYou do not have permission to use this command!", 59); 
                }
            } 
            else if (strncmp(buffer, "!who", 4) == 0) {
                pthread_mutex_lock(&userMutex);

                char cache[BUFFER_SIZE]; bzero(cache, 256);
                char cc[256] = "";
                bool ok = false;

                if (user->IsProfesor()) sprintf(cache, "\n\033[1mYou:\033[0m [\033[1;35mP\033[0m] %s %s\n\n", user->lastName, user->firstName);
                else sprintf(cache, "\n\033[1mYou\033[0m: [\033[1;36mS\033[0m] %s %s\n\n", user->lastName, user->firstName);

                for (auto it : users) {
                    bzero(cc, 256);

                    if (strcmp(it->ID, user->ID) != 0) {

                        if (it->IsProfesor()) sprintf(cc, "[\033[1;35mP\033[0m] %s %s\n", it->lastName, it->firstName);
                        else sprintf(cc, "[\033[1;36mS\033[0m] %s %s\n", it->lastName, it->firstName);
                    }

                    ok = true;
                    strcat(cache, cc);
                }

                pthread_mutex_unlock(&userMutex);

                write(user->socket, cache, strlen(cache));
                bzero(cache, BUFFER_SIZE);
            }
            else if (strncmp(buffer, "!help", 5) == 0) {
                user->Help();
            }
            else write(user->socket, "\033[31m[!]\033[0mUnknown command!", 28);
            
        } else {

            if (receiveLength > 0){

                if (user->IsProfesor()) sprintf(cache, "[\033[1;35mP\033[0m] %s %s: %s", user->lastName, user->firstName, buffer);
                else sprintf(cache, "[\033[1;36mS\033[0m] %s %s: %s", user->lastName, user->firstName, buffer);

                SendMessage(cache, user->ID);
		    } else {
                printf("[!]Server receive failed.");
			    chatMode = false;
            }
        }
	
    }

    return false;
} 

bool ExamRoom(User* user) {

    int count = GetQuestionsCount(user->examName);
    int index = 1;
    float points = 0;

    char buffer[BUFFER_SIZE];

    while (index <= count) {

        char* cache = GetQuestion(user->examName, index);

        write(user->socket, cache, strlen(cache));

        bzero(buffer, BUFFER_SIZE);
        int receiveLength = read(user->socket, buffer, BUFFER_SIZE);

        if (strncmp(buffer, "!exit", 5) == 0){

            sprintf(buffer, "\033[31m[-]\033[0m%s %s has left", user->lastName, user->firstName);
            printf("%s\n", buffer);
            SendMessage(buffer, user->ID);

            user->examRoom = false;

            return false;
        }

        points += CheckAnswer(buffer, index, user->examName);

        printf("%s %s a ales raspunsul %s la intrebarea %d.\n", user->lastName, user->firstName, buffer, index);

        index++;
    }

    write(user->socket, "End", 3);

    sleep(1);
    AddResult(user, points);

    user->examRoom = false;

    return true;
}


/* User Management Functions */
void AddUser (User *user) {
	pthread_mutex_lock(&userMutex);

	users.push_back(user);

	pthread_mutex_unlock(&userMutex);
}

void RemoveUser (string ID) {

	pthread_mutex_lock(&userMutex);

	users.remove_if([ID](User* user){ return user->ID == ID; });

	pthread_mutex_unlock(&userMutex);
}

void SendMessage (char *msg, char* ID) {
	pthread_mutex_lock(&userMutex);

    for (auto it : users) {
        if (strcmp(it->ID, ID) != 0) {
            if (it->examRoom == false) Check(write(it->socket, msg, strlen(msg)), "[!]Error at function sendMessage");
        }
    }

	pthread_mutex_unlock(&userMutex);
}


/* BD callback Functions */
static int callback_getUserInfo (void* data, int argc, char** argv, char** azColName) {

    User* user = (User*) data;

    if (argc > 0) {
        strcpy(user->ID, argv[0]);
        strcpy(user->firstName, argv[1]);
        strcpy(user->lastName, argv[2]);
        strcpy(user->grad, argv[3]);

        user->logged = true;
    }

    return 0;
}

static int callback_getQuestionsCount (void* data, int argc, char** argv, char** azColName) {

    int* count = (int*) data;

    *count = atoi(argv[0]);

    return 0;
}

static int callback_getQuestion (void* data, int argc, char** argv, char** azColName) {

    char* cache = (char*) data;
    bzero(cache, BUFFER_SIZE);

    sprintf(cache, "%s \n\t%s \n\t%s \n\t%s \n\t%s\n", argv[1], argv[2], argv[3], argv[4], argv[5]);

    return 0;
}

static int callback_getQuestionAnswer (void* data, int argc, char** argv, char** azColName) {

    char* cache = (char*) data;
    bzero(cache, 8);

    sprintf(cache, "%s", argv[6]);

    return 0;
}

static int callback_getResults (void* data, int argc, char** argv, char** azColName) {

    char* cache = (char*) data;
    char cc[BUFFER_SIZE]; bzero(cc, BUFFER_SIZE);

    sprintf(cc, "\t\033[1mExam:\033[0m %s   \033[1mProfesor:\033[0m %s   \033[1mDate:\033[0m %s   \033[1mPoints:\033[0m %s\n", argv[1], argv[2], argv[3], argv[4]);

    strcat(cache, cc);
    bzero(cc, BUFFER_SIZE);

    return 0;
}

static int callback_getExams (void* data, int argc, char** argv, char** azColName) {

    char* cache = (char*) data;

    if (strcmp(argv[1], "Results") != 0 && strcmp(argv[1], "USERS") != 0) {
        strcat(cache, argv[1]);
        strcat(cache, " ");
    }
    return 0;
}

/* Queries */
int GetQuestionsCount(string tableName) {
    string query = "SELECT COUNT(*) FROM " + tableName + ";";

    int* count = new int[1];
    sqlite3_exec(DB, query.c_str(), callback_getQuestionsCount, count, NULL);

    return *count;
}

char* GetQuestion(string examName, int count) {
    char* cache = new char[BUFFER_SIZE];
    bzero(cache, BUFFER_SIZE);

    string query = "SELECT * FROM " + examName + " WHERE ID = '" + std::to_string(count) + "';";

    if (sqlite3_exec(DB, query.c_str(), callback_getQuestion, cache, NULL) == SQLITE_OK) {
        return cache;
    }
    else {
        //cout << "NOT OK\n";
        return (char*)"NOT OK";
    }
}

char* GetQuestionAnswer(int questionNumber, string examName) {

    char* cache = new char[8];
    bzero(cache, 8);

    string query = "SELECT * FROM " + examName + " WHERE ID = '" + std::to_string(questionNumber) + "';";

    sqlite3_exec(DB, query.c_str(), callback_getQuestionAnswer, cache, NULL);

    return cache;
}

bool AddResult(User* user, float points) {

    time_t t = time(0); 
    struct tm * timeStruct = localtime(&t);
    string date = std::to_string(timeStruct->tm_mday) + "." 
                + std::to_string(timeStruct->tm_mon + 1) + "." 
                + std::to_string(timeStruct->tm_year + 1900) + " "
                + std::to_string(timeStruct->tm_hour) + ":"
                + std::to_string(timeStruct->tm_min) + ":" 
                + std::to_string(timeStruct->tm_sec);

    string query = "INSERT INTO Results VALUES('";
    query += user->ID;
    query += "', '";
    query += user->examName; 
    query += "', '";
    query += user->profesorName; 
    query += "', '";
    query += date;
    query += "', '";
    query += std::to_string(points); 
    query += "');";

    return sqlite3_exec(DB, query.c_str(), NULL, 0, NULL);
}

char* ShowResults(User* user) {
    
    char* cache = new char[BUFFER_SIZE]; bzero(cache, BUFFER_SIZE);

    string query = "SELECT * FROM Results WHERE ID = '";
    query += user->ID;
    query += "';";

    cout << query << endl;

    strcpy(cache, "\n");

    sqlite3_exec(DB, query.c_str(), callback_getResults, cache, NULL);

    return cache;
}

bool CreateExam(string examName) {

    string query =  "CREATE TABLE " + examName + "("
                    "ID TEXT PRIMARY KEY        NOT NULL, "
                    "QUESTION           TEXT    NOT NULL, "
                    "Option_1           TEXT    NOT NULL, "
                    "Option_2           TEXT    NOT NULL, "
                    "Option_3           TEXT    NOT NULL, "
                    "Option_4           TEXT    NOT NULL, "
                    "CORRECT_ANSWER     TEXT    NOT NULL);";

    return (sqlite3_exec(DB, query.c_str(), NULL, 0, NULL) == SQLITE_OK);
}

bool DropExam(string examName) {

    string query =  "DROP TABLE " + examName + ";";

    return (sqlite3_exec(DB, query.c_str(), NULL, 0, NULL) == SQLITE_OK);
}

bool InsertQuestion (string examName, string question, string option1, string option2, string option3, string option4, string correctAnswer) {
    
    int index = GetQuestionsCount(examName) + 1;
    string query = "INSERT INTO " + examName + " VALUES( '" + std::to_string(index) + "', '" + std::to_string(index) + ". " + question + "', 'a) " + option1 + "', 'b) " + option2 + "', 'c) " + option3 + "', 'd) " + option4+ "', '" + correctAnswer + "');";

    return (sqlite3_exec(DB, query.c_str(), NULL, 0, NULL) == SQLITE_OK);
}

bool UpdateQuestion (string questionNumber, string examName, string question, string option1, string option2, string option3, string option4, string correctAnswer) {

    string query = "UPDATE " + examName + " SET " + "QUESTION = '" + questionNumber + ". " +question + 
                                                    "' ,Option_1 = '" + "a) " + option1 + 
                                                    "' ,Option_2 = '" + "b) " + option2 + 
                                                    "' ,Option_3 = '" + "c) " + option3 + 
                                                    "' ,Option_4 = '" + "d) " + option4 +
                                                    "' ,CORRECT_ANSWER = '" + correctAnswer + "' WHERE ID = '" + questionNumber + "';";

    return (sqlite3_exec(DB, query.c_str(), NULL, 0, NULL) == SQLITE_OK);
}

bool RemoveQuestion (string examName, string questionNumber) {
    
    string query = "DELETE FROM " + examName + " WHERE ID = '" + questionNumber + "';";

    int index = atoi(questionNumber.c_str());

    int questionsContor = GetQuestionsCount(examName);

    if ((sqlite3_exec(DB, query.c_str(), NULL, 0, NULL) == SQLITE_OK) == false) return false;

    while (index < questionsContor) {
        string query_update = "UPDATE " + examName + " SET " + "ID = " + std::to_string(index) + " WHERE ID = " + std::to_string(index + 1) + ";";

        cout << query_update << endl;

        index++;

        if ((sqlite3_exec(DB, query_update.c_str(), NULL, 0, NULL) == SQLITE_OK) == false) return false;
    }

    return true;
}

bool ShowQuestions (string examName, int socket) {
    int count = GetQuestionsCount(examName);
    int index = 1;

    while (index <= count) {
        
        char* cache = GetQuestion(examName, index);

        if (strcmp(cache, "NOT OK") == 0) {
            return false;
        }

        write(socket, cache, strlen(cache));

        index++;
    } 

    return true;
}

bool ShowExams (int socket) {

    string query = "SELECT * FROM sqlite_master where type = 'table'";

    char cache[BUFFER_SIZE];

    strcpy(cache, "\033[1mExams: \033[0m");

    bool ok = sqlite3_exec(DB, query.c_str(), callback_getExams, cache, NULL);

    if (ok = true) {
        write(socket, cache, strlen(cache));
    }
    return ok;
}   

bool AddUser(string firstName, string lastName, string grad) {
    return true;
}

/* Check Functions */
float CheckAnswer(char* answer, int questionNumber, string examName) {

    char cache[8];
    strcpy(cache, GetQuestionAnswer(questionNumber, examName));

    int count = 0;
    bool ok = false;

    if (strlen(cache) == 1) {
        if (strcmp(answer, cache) == 0) count++;
    } else {
        for (int i = 0; i < strlen(cache); i++) {
            ok = false;

            for (int j = 0; j < strlen(answer) and !ok; j++) {
                if (cache[i] == answer[j]) ok = true;
            }

            if (ok) count++; 
            else count--;
        }
    }

    return (float)(std::max(count, 0) * (1.0f / (float)strlen(cache)));
}

bool CheckID(string ID, User* user) {

    for (int i = 0; i < ID.size(); i++) {
        if ( !(ID[i] >= '0' && ID[i] <= '9') ) return false;
    }

    pthread_mutex_lock(&userMutex);

    for (auto it : users) {
        if (strcmp(it->ID, ID.c_str()) == 0 ) return false;
    }

    pthread_mutex_unlock(&userMutex);

    string query = "SELECT * FROM USERS WHERE ID = '" + ID + "';";
    sqlite3_exec(DB, query.c_str(), callback_getUserInfo, user, NULL);

    return true;
}

int Check (int value, const char* errorMessage) {
    if (value < 0) {
        perror(errorMessage);
        exit(1);
    }

    return value;
}


/* Utility Functions */
void CreateUsersDB () {

    char* messageError;

    string query_drop = "DROP TABLE USERS;";

    if (sqlite3_exec(DB, query_drop.c_str(), NULL, 0, &messageError) != SQLITE_OK) 
        printf("\033[31m[!]\033[0mDrop table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully dropted.\n");

     string query_create =    "CREATE TABLE USERS("
                            "ID TEXT PRIMARY KEY        NOT NULL, "
                            "FIRST_NAME         TEXT    NOT NULL, "
                            "LAST_NAME          TEXT    NOT NULL, "
                            "GRAD               TEXT    NOT NULL);";

    if (sqlite3_exec(DB, query_create.c_str(), NULL, 0, &messageError) != SQLITE_OK)
        printf("\033[31m[!]\033[0mCreate table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully created.\n");

    string query_insert("INSERT INTO USERS VALUES('1000', 'Admin', 'Admin', 'Profesor');"
               "INSERT INTO USERS VALUES('1001', 'Bogdan', 'Patrut', 'Profesor');"
               "INSERT INTO USERS VALUES('1002', 'Ioana', 'Bogdan', 'Profesor');"
               "INSERT INTO USERS VALUES('0001', 'Leonard', 'Rumeghea', 'Student');"
               "INSERT INTO USERS VALUES('0002', 'Sabin', 'Chirila', 'Student');"
               "INSERT INTO USERS VALUES('0003', 'Antonio', 'Iatu', 'Student');"
               "INSERT INTO USERS VALUES('0004', 'Marin', 'Hricium', 'Student');"
               "INSERT INTO USERS VALUES('0005', 'Alexandru', 'Ceica', 'Student');"
               "INSERT INTO USERS VALUES('0006', 'Bianca', 'Nazare', 'Student');");
  
    if (sqlite3_exec(DB, query_insert.c_str(), NULL, 0, &messageError) != SQLITE_OK) 
        printf("\033[31m[!]\033[0mTable insert failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully inserted\n");

    query_drop = "DROP TABLE SesiuneRdC;";

    if (sqlite3_exec(DB, query_drop.c_str(), NULL, 0, &messageError) != SQLITE_OK) 
        printf("\033[31m[!]\033[0mDrop table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully dropted.\n");

    query_create =  "CREATE TABLE SesiuneRdC("
                    "ID TEXT PRIMARY KEY        NOT NULL, "
                    "QUESTION           TEXT    NOT NULL, "
                    "Option_1           TEXT    NOT NULL, "
                    "Option_2           TEXT    NOT NULL, "
                    "Option_3           TEXT    NOT NULL, "
                    "Option_4           TEXT    NOT NULL, "
                    "CORRECT_ANSWER     TEXT    NOT NULL);";

    if (sqlite3_exec(DB, query_create.c_str(), NULL, 0, &messageError) != SQLITE_OK)
        printf("\033[31m[!]\033[0mCreate table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully created.\n");

    query_insert = {"INSERT INTO SesiuneRdC VALUES('1', 'I. Raspunde a.', 'a) a', 'b) b', 'c) c', 'd) d', 'a');"
                 "INSERT INTO SesiuneRdC VALUES('2', 'II. Raspunde b.', 'a) a', 'b) b', 'c) c', 'd) d', 'b');"
                 "INSERT INTO SesiuneRdC VALUES('3', 'III. Raspunde bc.', 'a) a', 'b) b', 'c) c', 'd) d', 'bc');"
                 "INSERT INTO SesiuneRdC VALUES('4', 'IV. Raspunde abc.', 'a) a', 'b) b', 'c) c', 'd) d', 'abc');"};
  
    if (sqlite3_exec(DB, query_insert.c_str(), NULL, 0, &messageError) != SQLITE_OK) 
        printf("\033[31m[!]\033[0mTable insert failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully inserted\n");

    query_drop = "DROP TABLE Results;";

    if (sqlite3_exec(DB, query_drop.c_str(), NULL, 0, &messageError) != SQLITE_OK) 
        printf("\033[31m[!]\033[0mDrop table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully dropted.\n");

    query_create =  "CREATE TABLE Results("
                    "ID                 TEXT    NOT NULL, "
                    "EXAM_NAME          TEXT    NOT NULL, "
                    "DATE               TEXT    NOT NULL, "
                    "PROFESOR           TEXT    NOT NULL, "
                    "RESULT             TEXT    NOT NULL);";

    if (sqlite3_exec(DB, query_create.c_str(), NULL, 0, &messageError) != SQLITE_OK)
        printf("\033[31m[!]\033[0mCreate table failed: %s.\n", messageError);
    else printf("\033[32m[+]\033[0mTable successfully created.\n");
}