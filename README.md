# QuizzGame

Server: g++ server.cpp User.cpp -o server -lsqlite3 -pthread
Client: g++ client.cpp -o client -pthread

g++ server.cpp User.cpp -o server -lsqlite3 -pthread | g++ client.cpp -o client -pthread

To log in, use one of these IDs:
    Professor: 1001 1002 1000
    Student: 0001 0002 0003 0004 0005 0006

Use the command !help to see a list of commands.

    Professor:
            !create examName -> This command produces an exam with the specified name
            !drop examName -> This command removes an exam with the specified name
            !insert -> This command adds a question to an exam
            !update -> This command update a question in an exam
            !remove -> This command removes a question from an exam
            !show -> This command displayed the exams
            !show examName -> This command displayed exam questions with the specified name
            !who -> This command provides a list of persons that are currently connected to the server

    Student:
            !results -> This command displays results from previous exams
            !who -> This command provides a list of persons that are currently connected to the server
