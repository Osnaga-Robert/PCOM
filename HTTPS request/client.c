#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    int sockfd;
    int check_login = 0;
    int check_library = 0;
    int type = -1;

    //buffere pentru input,cookie,token
    char input[30];
    char cookie[500];
    char cookie1[500];
    char token[300];
    memset(token,0,sizeof(token));


    while (1)
    {
        //dorim sa rulam non-stop informatiile, deci vom face un while

        fflush(stdin);
        //citim input-ul
        fgets(input,sizeof(input),stdin);
        input[strlen(input) - 1] = '\0';
        if (strcmp(input, "register") == 0)
        {
            //cazul in care se apeleaza register luam datele aferente
            char username[50];
            char password[50];
            printf("username=");
            fgets(username,sizeof(username),stdin);
            username[strlen(username) - 1] = '\0';
            printf("password=");
            fgets(password,sizeof(password),stdin);
            password[strlen(password) - 1] = '\0';

            //cream JSON-ul cu username si password
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);

            //deschidem socket-ul si prelucram mesajul de POST
            sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
            message = compute_post_request("34.254.242.81", "/api/v1/tema/auth/register", "application/json", root_value, 1, NULL, 0, -1);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            close_connection(sockfd);
            
            //decodificam raspunsul de la server
            if(strstr(response,"400 Bad Request") != NULL)
                printf("%s\n","The username already exists!\n");
            else
                printf("Your account has been registered!\n");

            json_value_free(root_value);
        }
        else if (strcmp(input, "login") == 0)
        {
            //cazul in care se apeleaza login luam datele aferente
            char username[30];
            char password[30];
            printf("username=");
            fgets(username,sizeof(username),stdin);
            username[strlen(username) - 1] = '\0';
            printf("password=");
            fgets(password,sizeof(password),stdin);
            password[strlen(password) - 1] = '\0';

            //cream JSON-ul cu username si password
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);
            char *serialized_string = json_serialize_to_string_pretty(root_value);

            //deschidem socket-ul si prelucram mesajul de POST
            sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
            message = compute_post_request("34.254.242.81", "/api/v1/tema/auth/login", "application/json", root_value, 1, NULL, 0, -1);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            //decodificam raspunsul de la server
            if(strstr(response,"400 Bad Request") != NULL)
                printf("The password is wrong or the username does not exist!\n");
            else
                printf("You entered in your account!\n");
                

            //prelucram cookie-ul in cazul in care cineva s-a conectat pe server
            if (strstr(response, "200 OK") != NULL)
            {
                check_login = 1;
                // extract the cookie
                char *p = strtok(response, "\n");
                while (p != NULL)
                {
                    if (strstr(p, "Set-Cookie") != NULL)
                    {
                        strcpy(cookie1, p);
                        break;
                    }
                    p = strtok(NULL, "\n");
                }
                char *new = strstr(cookie1, "connect.sid");
                char* newnew = strtok(new, ";");
                memset(cookie,0,sizeof(cookie));
                // memmove(newnew, newnew + 12, strlen(newnew) - 12 + 1);
                strcpy(cookie, newnew);
            }
            close_connection(sockfd);   

            json_free_serialized_string(serialized_string); // free the serialized string
            json_value_free(root_value); 
        }
        else if (strcmp(input, "enter_library") == 0)
        {
            //in cazul in care dorim sa intram in librarie
            type = 1;
            if(check_login == 1){
                //deschidem conexiunea si prelucram mesajul de GET    
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_get_request("34.254.242.81", "/api/v1/tema/library/access", NULL, cookie, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);
                close_connection(sockfd);

                //prelucram raspunsul si luam token-ul
                if(strstr(response,"401 Unauthorized") != NULL)
                    printf("%s\n","You are not logged in");
                else{
                    printf("You entered to the library\n");
                    strcpy(token, response);
                    char *test = basic_extract_json_response(token);
                    strcpy(token, test + 10);
                    token[strlen(token) - 2] = '\0';
                    check_library = 1;
                }
                
            }    
            else{
                printf("You are not logged in!\n");
            }        
        }
        else if (strcmp(input, "get_books") == 0)
        {
            //pentru a afisa toate cartile
            if(check_library == 1 && check_login == 1){
                //dechidem conexiunea si afisam raspunsul de la server, dupa care inchidem conexiunea
                type = 2;
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_get_request("34.254.242.81", "/api/v1/tema/library/books", NULL, token, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);
                close_connection(sockfd);

                printf("%s\n",response);
    
            }
            else if(check_login == 0){
                printf("You are not logged in!\n");
            }
            else{
                printf("You are not in the library!\n");
            }
            
        }
        else if (strcmp(input, "get_book") == 0)
        {
            //in cazul in care dorim o carte anume
            if(check_login == 1 && check_library == 1){
                type = 2;
                char id[10];
                int ok = 0;
                //luam id-ul si verificam daca este corect
                printf("id=");
                fgets(id,sizeof(id),stdin);
                id[strlen(id) - 1] = '\0';
                for(int i = 0 ; i < strlen(id) ; i++){
                    if(id[i] < '0' || id[i] > '9'){
                        printf("Wrong data!\n");
                        ok = 1;
                        break;
                    }
                }
                if(ok == 1)
                    continue;  
                char ruta[50];
                strcpy(ruta, "/api/v1/tema/library/books/");
                strcat(ruta, id);


                //deschidem conexiunea, trimitem mesajul catre server si prelucram raspunsul
                //dupa care inchidem conexiunea
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_get_request("34.254.242.81", ruta, NULL, token, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);

                if(strstr(response,"400 Bad Request") != NULL)
                    printf("%s\n","You entered an invalid ID");
                else{
                    printf("Here is your book:");
                    printf("%s\n",basic_extract_json_response(response));
                }

                close_connection(sockfd);
            }
            else if(check_login == 0){
                printf("You are not logged in!\n");
            }
            else{
                printf("You are not in the library!\n");
            }


        }
        else if (strcmp(input, "add_book") == 0)
        {
            //in cazul in care adaugam o carte
            if(check_login == 1 && check_library == 1){
                
                type = 3;
                char title[100];
                char author[100];
                char genre[100];
                char publisher[100];
                char page_count[100];
                //luam datele de la stdin
                
                printf("title=");
                fgets(title,sizeof(author),stdin);
                title[strlen(title) - 1] = '\0';
                printf("author=");
                fgets(author,sizeof(author),stdin);
                author[strlen(author) - 1] = '\0';
                printf("genre=");
                fgets(genre,sizeof(author),stdin);
                genre[strlen(genre) - 1] = '\0';
                printf("publisher=");
                fgets(publisher,sizeof(author),stdin);
                publisher[strlen(publisher) - 1] = '\0';
                printf("page_count=");
                fgets(page_count,sizeof(author),stdin);
                page_count[strlen(page_count) - 1] = '\0';

                //verificam daca paginile au un input corect
                int ok = 0;
                for(int i = 0 ; i < strlen(page_count) ; i++){
                    if(page_count[i] < '0' || page_count[i] > '9'){
                        printf("Wrong data!\n");
                        ok = 1;
                        break;
                    }
                }
                if(ok == 1)
                    continue;  


                //le intriducem intr-un JSON
                JSON_Value *root_value = json_value_init_object();
                JSON_Object *root_object = json_value_get_object(root_value);
                json_object_set_string(root_object, "title", title);
                json_object_set_string(root_object, "author", author);
                json_object_set_string(root_object, "genre", genre);
                json_object_set_string(root_object, "publisher", publisher);
                json_object_set_string(root_object, "page_count", page_count);

                //trimitem la server datele si primim raspunsul pe care il afisam
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_post_request("34.254.242.81", "/api/v1/tema/library/books", "application/json", root_value, 1, token, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);

                printf("%s\n",response);

                close_connection(sockfd);

            }
            else if(check_login == 0){
                printf("You are not logged in!\n");
            }
            else{
                printf("You are not in the library!\n");
            }
            
        }
        else if (strcmp(input, "delete_book") == 0)
        {
            //daca dorim sa stergem o carte
            if(check_login == 1 && check_library == 1){
                type = 2;
                char id[30];
                printf("id=");
                fgets(id,sizeof(id),stdin);
                id[strlen(id) - 1] = '\0';
                //verificam id-ul dat de la tastatura
                int ok = 0;
                for(int i = 0 ; i < strlen(id) ; i++){
                    if(id[i] < '0' || id[i] > '9'){
                        printf("Wrong data!\n");
                        ok = 1;
                        break;
                    }
                }
                if(ok == 1)
                    continue;  
                char ruta[60];
                strcpy(ruta, "/api/v1/tema/library/books/");
                strcat(ruta, id);
                //deschidem conectiunea cu server-ul,trimitem datele si prelucram raspunsul
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_delete_request("34.254.242.81", ruta, NULL, token, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);
                close_connection(sockfd);
                if(strstr(response,"404 Not Found") != NULL){
                    printf("The ID does not exist\n");
                }
                else if (strstr(response,"500 Internal Server Error") != NULL){
                    printf("The ID is wrong\n");
                }
                else{
                    printf("The book was deleted\n");
                }

            }
            else if(check_login == 0){
                printf("You are not logged in!\n");
            }
            else{
                printf("You are not in the library!\n");
            }
            
            
        }
        else if (strcmp(input, "logout") == 0)
        {
            //la logout verificam daca suntem logati sau nu
            if(check_login == 1){
                //trimitem cererea de log_out la server
                check_library = 0;
                check_login = 0;
                strcpy(token, "");
                type = 1;
                sockfd = open_connection("34.254.242.81", 8080, AF_INET, SOCK_STREAM, 0);
                message = compute_get_request("34.254.242.81", "/api/v1/tema/auth/logout", NULL, cookie, 0, type);
                send_to_server(sockfd, message);
                response = receive_from_server(sockfd);
                close_connection(sockfd);
                printf("Goodbye!\n");
            }
            else{
                printf("You are not logged in\n");
            }
            
        }
        else if (strcmp(input, "exit") == 0)
        {
            //in cazul in care se va da exit, vom iesi.
            return 0;
        }
    }

    return 0;
}
