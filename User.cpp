#include <list>
#include <string.h>
#include <string>
#include <unistd.h>
#include "User.h"

using namespace std;

User::User() {
    this->logged = false;
    this->examRoom = false;
}

bool User::IsProfessor() { return strcmp(this->grad, "Professor") == 0; }

bool User::IsStudent() { return strcmp(this->grad, "Student") == 0; }

void User::Help() {
    if (this->IsProfessor()) {
        string cache =  "\n   \033[1m!\033[0m\033[36mcreate\033[0m [examName] -> This command produces an exam with the specified name\n"
                        "   \033[1m!\033[0m\033[36mdrop\033[0m [examName]-> This command removes an exam with the specified name\n"
                        "   \033[1m!\033[0m\033[36minsert\033[0m -> This command adds a question to an exam\n"
                        "   \033[1m!\033[0m\033[36mupdate\033[0m -> This command update a question in an exam\n"
                        "   \033[1m!\033[0m\033[36mremove\033[0m -> This command removes a question from an exam\n"
                        "   \033[1m!\033[0m\033[36mshow\033[0m -> This command displayed the exams\n"
                        "   \033[1m!\033[0m\033[36mshow\033[0m [examName]-> This command displayed exam questions with the specified name\n"
                        "   \033[1m!\033[0m\033[36mwho\033[0m -> This command provides a list of persons that are currently connected to the server\n";

        write(this->socket, cache.c_str(), cache.size());

    }else {
        string cache =  "\n   \033[1m!\033[0m\033[36mresults\033[0m -> This command displays results from previous exams\n"
                        "   \033[1m!\033[0m\033[36mwho\033[0m -> This command provides a list of persons that are currently connected to the server\n";

        write(this->socket, cache.c_str(), cache.size());
    }
}
