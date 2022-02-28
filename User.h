#include <list>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <pthread.h>

class User {
private:

public:
    pthread_t thread;
    struct sockaddr_in address;
	int socket;

    bool logged = false;

    char examName[32];
    char profesorName[65];
    bool examRoom = false;
	
    char ID[8];
    char firstName[32];
    char lastName[32];
    char grad[16];

    User();

    bool IsProfesor();
    bool IsStudent();
    void Help();
};