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
/*  Pentru a fi sigur de existenta conexiunii, in fiecare functie ce corespunde unei comenzi se descide o conexiune noua cu
    serverul, ce se inchide ulterior la finalul executiei functiei */

    /*Functie ce implementeaza inregistrarea*/
int inregistrare(char **cookies) {
    int sockfd;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0); //server-ip was originally the ip of the server
    char *message;
    char *response;
    char username[50];
    char password[50];
    char *data;
    int ret = 1;
    if(cookies[0]){
        printf("Sunteti deja locat\n");
        return 0;
    }
    //  Se introduce imputul de la user
    printf("username:");
    fgets(username, 50, stdin); 
    username[strcspn(username, "\n")] = 0;
    printf("password:");
    fgets(password, 50, stdin); 
    password[strcspn(password, "\n")] = 0;

    //  Se creaza un obiect JSON cu informatiile introduse, se transforma in string si este
    //  dat ca imput in mesaj
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root = json_value_get_object(root_value);    
    json_object_set_string(root, "username", username);
    
    json_object_set_string(root, "password", password);
    data = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value);
   
    //  se compune mesajul de tip POST
    message = compute_post_request("server-ip", "/api/v1/tema/auth/register", "application/json", data, NULL, 1, NULL);

    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 400 Bad Request", response, 24) == 0){
        printf("User already exists\n");
        ret = 0;
    } else 
        printf("Registration successful\n");
    free(message);
    free(response);
    json_free_serialized_string(data);

    return ret;
}
    /*Functie ce implementeaza autentificarea. Returneaza cookie-ul de autentificare ca sir de caractere*/
char *login() {
    char *message;
    char *response;
    int sockfd;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);    

    char username[50];
    char password[50];
    char *data;
    //  Se introduce imputul de la user
    printf("username:");
    fgets(username, 50, stdin); 
    username[strcspn(username, "\n")] = 0;
    printf("password:");
    fgets(password, 50, stdin); 
    password[strcspn(password, "\n")] = 0;
    //  Se creaza un obiect JSON cu informatiile introduse, se transforma in string si este
    //  dat ca imput in mesaj
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root = json_value_get_object(root_value);   
    json_object_set_string(root, "username", username);
    
    json_object_set_string(root, "password", password);
    data = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value);
    //  se compune mesajul de tip POST
    message = compute_post_request("server-ip", "/api/v1/tema/auth/login", "application/json", data, NULL, 1, NULL);
    json_free_serialized_string(data);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 400 Bad Request", response, 24) == 0) {
        free(message);
        free(response);
        printf("The user doesn't exists\n");
        return NULL;
    }
    printf("Login successful with username %s\n", username);
    char *cookie = malloc(LINELEN);
    char *cs =strstr(response,"Set-Cookie: ")+ 12;
    int cf =(int) (strchr(cs, ';')-cs);
    strncpy(cookie, cs, cf); 
    free(message);
    free(response);
    return cookie;

}
    /*Functie ce implementeaza functionalitatea enter_library. Returneaza tokenul JWT(ca sir de caractere)*/   
char *enter_library(char **cookies, int cl) {
    char *message;
    char *response;
    int sockfd;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    /*  se compune mesajul de tip get, se folosesc char **cookies care este un vector
        ce contine toate cookies, ce daca userul este logat va contine cookiul ce dovedeste
        ca userul este logat.*/
    message = compute_get_request("server-ip", "/api/v1/tema/library/access", NULL, cookies, cl, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 401 Unauthorized" , response, 25) == 0) {
        printf("You are not logged in\n");
        free(message);
        free(response);
        return NULL;
    }
    printf("You've entered the library\n");
    char *tokenaddr = strstr(response, "token") + 8;
    char *token = calloc(LINELEN,1);
    int to_copy = (int) (strchr(tokenaddr, '}') - tokenaddr) - 1;
    strncpy(token, tokenaddr, to_copy);
    free(message);
    free(response);
    return token;
}
    /*Functie ce implementeaza funtionalitatea get_books */
int get_books(char *token, char **cookies) {
    char *message;
    char *payload = calloc(BUFLEN, 1);
    char *response;
    int sockfd;
    int ret = 1;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    message = compute_get_request("server-ip", "/api/v1/tema/library/books", NULL, cookies, 1, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 500 Internal Server Error", response, 34) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    }
    else if(strncmp("HTTP/1.1 403 Forbidden", response, 22) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    } else {
    /*  Se parseaza JSON-ul primit ca payload al raspunsului si se interpreteaza.
        Va afisa toate titlurile si id-urilor cartilor prezente in biblioteca*/
        char *pa = strchr(response, '[');
        int cf = (int) (strchr(pa,']') - pa);
        strncpy(payload, pa, cf+1);
        JSON_Value *root_value;
        JSON_Array *list;
        JSON_Object *book;
        root_value = json_parse_string(payload);

        list = json_value_get_array(root_value);
        if(!json_array_get_count(list))
            printf("There are no books in the library\n");
        else
            printf("Books:\n");
        for(int i = 0 ; i< json_array_get_count(list) ; i++) {
            book = json_array_get_object(list, i);  
            printf("Title: %s,", json_object_get_string(book, "title")) ;
            printf("ID: %.0lf\n", json_object_get_number(book, "id"));
        }
        json_value_free(root_value);
    }

    free(message);
    free(response);
    free(payload);
    return ret;
}
    /*Functie ce implementeaza funtionalitatea get_books */
int add_book(char *token, char **cookies) {
    char *message;
    char *response;
    int sockfd;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    char title[200];
    char author[200];
    char genre[50];
    char publisher[200];
    int page_count;
    char page_counts[50];
    char *data;
    //  Se introduc date despre cartea ce va fi adaugata de catre user
    printf("title:");
    fgets(title, 200, stdin);
    title[strcspn(title, "\n")] = 0;
    printf("author:");
    fgets(author, 200, stdin);
    author[strcspn(author, "\n")] = 0;
    printf("genre:");
    fgets(genre, 200, stdin);
    genre[strcspn(genre, "\n")] = 0;
    printf("publisher:");
    fgets(publisher, 200, stdin);
    publisher[strcspn(publisher, "\n")] = 0;
    printf("page_count:");
    fgets(page_counts, 50, stdin); 
    page_counts[strcspn(page_counts, "\n")] = 0;
    page_count = atoi(page_counts);
    //  Se creaza obiectul JSON cu datele introduse, se converteste rezultatul in
    //  sirul de caractere data
    JSON_Value *root_value = json_value_init_object();
    JSON_Object *root = json_value_get_object(root_value);  
    json_object_set_string(root, "title", title);
    json_object_set_string(root, "author", author);
    json_object_set_string(root, "genre", genre);
    json_object_set_number(root, "page_count", page_count);
    json_object_set_string(root, "publisher", publisher);
    data = json_serialize_to_string_pretty(root_value);
    json_value_free(root_value);
    //  Se cokmpune mesajul POST catre server
    message = compute_post_request("server-ip", "/api/v1/tema/library/books", "application/json", data, cookies, 1, token);
    json_free_serialized_string(data);

    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 403 Forbidden", response, 22) == 0) {
        printf("You don't have access to the library\n");
        free(message);
        free(response);
        close(sockfd);
        return 0;
    }    
    if(strncmp("HTTP/1.1 500 Internal Server Error", response, 34) == 0) {
        printf("You don't have access to the library\n");
        free(message);
        free(response);
        close(sockfd);
        return 0;
    }
    if(strncmp("HTTP/1.1 400 Bad Request", response, 24) == 0) {
        free(message);
        free(response);
        close(sockfd);
        printf("Wrong request\n");
        return 0;
    }

    printf("The book was added successfully\n");
    free(message);
    free(response);
    close(sockfd);
    return 1;

}
    /*Functie ce implementeaza funtionalitatea get_book */
int get_book(char *token, char **cookies) {
    char *message;
    char *response;
    char *payload = calloc(BUFLEN, 1);
    int sockfd;
    int ret = 1;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    //  Utilizatorul introduce id-ul cartii in cauza
    char id[10];
    printf("id=");
    fgets(id, 10, stdin);
    id[strcspn(id, "\n")] = 0;

    char url[100] = "/api/v1/tema/library/books/";
    //  Se creaza url-ul dorit
    strcat(url,id);
    //  Se compune mesajul de tip GET
    message = compute_get_request("server-ip", url, NULL, cookies, 1, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    if(strncmp("HTTP/1.1 403 Forbidden", response, 22) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    }    
    else if(strncmp("HTTP/1.1 500 Internal Server Error", response, 34) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    }
    else if(strncmp("HTTP/1.1 404 Not Found", response, 22) == 0) {
        printf("The book you're looking for doesn't exists\n");
        ret = 0;
    } else {
        //  Daca nu exista erori se va interpreta payload-ul primit de la server,
        //  se va parsa intr-un JSON si se vor afisa date despre cartea dorita
        char *pa = strchr(response, '[');
        int cf = (int) (strchr(pa,']') - pa);
        strncpy(payload, pa, cf+1);
        JSON_Value *root_value;
        JSON_Object *book;
        JSON_Array *list;
        root_value = json_parse_string(payload);
        
        list = json_value_get_array(root_value);

        book = json_array_get_object(list, 0);  
    
        printf("Title: %s\n", json_object_get_string(book, "title")) ;
        printf("Author: %s\n", json_object_get_string(book, "author")) ;
        printf("Publisher: %s\n", json_object_get_string(book, "publisher"));
        printf("Genre: %s\n", json_object_get_string(book, "genre")) ;
        printf("Page count: %.0lf\n", json_object_get_number(book, "page_count"));
        json_value_free(root_value);
    
    }
    close(sockfd);
    free(message);
    free(response);

    return ret;
}
    /*Functie ce implementeaza funtionalitatea delete_book */
int delete_book(char *token ,char **cookies) {
    char *message;
    char *response;
    int sockfd;
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    char id[10];
    int ret = 1;
    printf("id=");
    fgets(id, 10, stdin);
    id[strcspn(id, "\n")] = 0;

    char url[100] = "/api/v1/tema/library/books/";
    //  Se compune url-ul 
    strcat(url,id);
    //  Se trimite mesajul catre server
    message = compute_delete_request("server-ip", url, cookies, 1, token);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    close(sockfd);
    if(strncmp("HTTP/1.1 403 Forbidden", response, 22) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    }    
    else if(strncmp("HTTP/1.1 500 Internal Server Error", response, 34) == 0) {
        printf("You don't have access to the library\n");
        ret = 0;
    }
    else if(strncmp("HTTP/1.1 404 Not Found", response, 22) == 0) {
        printf("The book you're looking for doesn't exists\n");
        ret = 0;
    } 
    else 
        printf("The book was deleted successfully\n");   
    free(message);
    free(response);
    return ret;
}
    /*Functie ce implementeaza funtionalitatea logout */
int logout(char **cookies, int *cl) {
    char *message;
    char *response;
    int sockfd;
    if(!cookies[0]) {
        printf("You are not logged in\n");
        return 0;
    }
    sockfd = open_connection((char*)"server-ip", 8080, AF_INET, SOCK_STREAM, 0);
    //  Se trimite mesajul catre server 
    message = compute_get_request("server-ip", "/api/v1/tema/auth/logout", NULL, cookies, 1, NULL);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    free(message);
    free(response); 
    free(cookies[0]); 
    cookies[0] = NULL;
    printf("You've logged out successfully\n");
    *cl--;
    return 1;
}
int main(int argc, char *argv[])
{

    char **cookies = calloc(1, sizeof(char *));
    char *authorization = NULL;
    int cl = 1;
    while(1) {
        char cmd[30];
        fgets(cmd, 30, stdin);

        if(strncmp(cmd,"register", 8) == 0 && !cookies[0]) {
            inregistrare(cookies);
            continue;
        }
        if(strncmp(cmd,"login", 5) == 0) { 
            if(cookies[0]) {
                printf("Already logged in\n");
                continue;
            }
            char *c = login();
            if(c) {
                cookies[0] = calloc(LINELEN, sizeof(char));
                strcpy(cookies[0], c);
                free(c);
            }
            continue;
        }
        if(strncmp(cmd, "add_book", 8) == 0) {
            add_book(authorization, cookies);
            continue;
        }
        if(strncmp(cmd, "enter_library", 13) == 0) {
            authorization = enter_library(cookies, cl);
            continue;
        }
        if(strncmp(cmd, "get_books", 9) == 0) {
            get_books(authorization, cookies);
            continue;
        }
        if(strncmp(cmd, "get_book", 8) == 0) {
            get_book(authorization, cookies);
            continue;
        } 
        if(strncmp(cmd, "delete_book", 11) == 0) {
            delete_book(authorization, cookies);
            continue;
        }       
        if(strncmp(cmd, "logout", 6) == 0) {
            logout(cookies, &cl);
            continue;
        }
        if(strncmp(cmd, "exit", 4) == 0)
            break;
    }

    for(int i = 0; i< cl; i++)
        free(cookies[i]);
    free(cookies);
    if(authorization)
        free(authorization);
    return 0;
}
