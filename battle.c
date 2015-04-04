#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#ifndef PORT
  #define PORT 12222
#endif
#define SIZE 20


struct client {
    int fd;
    char * name;
    struct in_addr ipaddr;
    struct client *next;

    // points to the opponent that the client has last battled with
    struct client *last_battled;
   
    // points to the client's opponent
    struct client * current;

    // number of hitpoints that the client has
    int hitpoints;

    // number of powermoves that the client has
    int powermoves;
    
    /* newline_received's value is 0 when the client hasn't finished entering 
     * his name yet, it's 1 when the client has finished entered his name 
     * already
     */
    int newline_received;

    /* active's value is 0 if the client is the nonactive_player, it's 1 if the
     * client is the active_player
     */
    int active;

    char *message;
    int i;
};


static struct client *addclient(struct client *top, int fd, 
                                 struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top);
int bindandlisten(void);

void display_hitpoints(int hitpoints, struct client *p);
void display_powermoves(int powermoves, struct client *p);
void display_opponents_hitpoints(int hitpoints, struct client *p,
                                struct client * opponent);
void display_info(struct client *p1, struct client *p2);
void display_moves(struct client *p1, struct client *p2);

int attack_generator(void);
void intialize_number_generator(void);
int hitpoints_generator(void);
int powermoves_generator(void);
int powerchance_generator(void);
int random_number(int max_number);

int combat(struct client *p1, struct client *p2, char *choice);
void initialize_battle(struct client *p1, struct client *p2);
int check_hitpoints(struct client *active_player, 
                    struct client *nonactive_player); 

struct client *  move_to_end(struct client *top, struct client *move);
struct client *find_last(struct client *top);

void check_for_malloc_error(int i);
void check_for_system_error(int i);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;
    
    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset); // initializes the set pointed to by allset to be empty
    // adds the file descriptor listenfd to the set pointed to by allset
    FD_SET(listenfd, &allset);     
    // maxfd identifies how far into the set to search
    maxfd = listenfd;
    
    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
         * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */
        
        // select system call blocks until one or more of a set file descriptors
        // becomes ready

        // &rset tested to see if input is possible
        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }
        
        if (nready == -1) {
            perror("select");
            continue;
        }
        
        if (FD_ISSET(listenfd, &rset)){ // is listenfd in set rset
            len = sizeof(q);
            if ((clientfd = accept(listenfd, 
                                  (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
	    // printf("connection from %s\n", inet_ntoa(q.sin_addr));
            
            // asks client that has just connected for their name
            char buf[20] = "What is your name? ";             
            write(clientfd, buf, sizeof(buf));

            // adds client to client list
            head = addclient(head, clientfd, q.sin_addr);         }       
            
            int i;
            struct client *p;
            for (i = 0; i <= maxfd; i++) {
                if (FD_ISSET(i, &rset)) {
                    for (p = head; p != NULL; p = p->next) {
                        if (p->fd == i) {
		            
                            // checks to see if a newline has been received 
                            // by client, ie whether they've given their 
                            // name or not
                            if (p->newline_received == 0) { 	  
			      char * buffer = malloc(sizeof(20));
			      // read into buffer
                                int nbytes = read(p->fd, buffer, 20);
                                // see if buffer contains newline
                                char * a = strchr(buffer, '\n'); // a is the location of newline
                                if (a != NULL) { // buffer has newline
				    p->newline_received = 1; 
                                    if (p->name == NULL) { 
                                        p->name = malloc(sizeof(20));
                                        *a = '\0';
                                        p->name = buffer; // set p->name to buffer
                                    } else if (strlen(p->name) < 20) {
				        *a = '\0'; // change newline to null terminator
                                        if ((strlen(p->name) 
                                             + strlen(buffer)) < 20) {
                                            strcat(&(p->name
                                                   [strlen(p->name)]), 
                                                   buffer);
                                        } else {
                                            strcat(&(p->name
                                                   [strlen(p->name)]), 
                                                   buffer);
                                            p->name[19] = '\0';
                                        }
                                    }
                                    // since client has entered newline we 
				    // can present with the greeting
                                    char part[50];
                                    sprintf(part, "Welcome, %s! Awaiting "
                                            "opponent...\r\n", p->name);
                                    write(p->fd, part, strlen(part));
                                    char mes[50];
                                    sprintf(mes, "**%s enters the arena**"
                                            "\r\n", p->name);
				    // notify all clients that a new client as
				    // entered the arena
                                    broadcast(head->next, mes, strlen(mes));
                 
                                 } else { // buffer does not have newline
                                    if (p->name == NULL) {
                                        p->name = malloc(sizeof(20));
                                        p->name = buffer;
                                        p->name[nbytes] = '\0';
                                   
                                    } else if (strlen(p->name) < 20) {
                                        strcat(&(p->name[strlen(p->name)]), 
                                               buffer);
                                        if ((strlen(p->name)+nbytes) < 20) {
                                            p->name[strlen(p->name) 
                                                    + nbytes] = '\0';
                                        } else {
                                            p->name[19] = '\0';
                                        }
                                    }
                                }  
           
                            // player that is in a game and whose turn it is
                            } else if ((p->current != NULL) 
                                       && (p->active == 1)) {
                                char * choice = malloc(sizeof(char)*50);
                                int result = read(p->fd, choice, 
                                                  sizeof(choice));  
                                if (result == 0) { // active player dropped
                                    char buf[100]; 
                                    sprintf(buf, "--%s dropped. You win!\n\n"
                                            "Awaiting next opponent...\r\n", 
                                            p->name);
                                    write((p->current)->fd, buf, strlen(buf));
                                    char drop[50];
                                    sprintf(drop, "**%s leaves**\r\n", p->name);
                                    broadcast(head, drop, strlen(drop));
                                    (p->current)->last_battled  = NULL;
                                    (p->current)->current = NULL;
                    
                                    int tmp_fd = p->fd;
                                    head = removeclient(head, p->fd);
                                    FD_CLR(tmp_fd, &allset);
                                    close(tmp_fd);
	                   
                                } else { // intiate combat
			            int re = combat(p, p->current, choice);
			   
                                    if (re == 1) { // battle ended normally 
                                        (p->current)->last_battled  = p;
                                        p->last_battled = p->current;
                                        struct client * temp = p->current;
                                        p->current = NULL;
                                        temp->current = NULL;
                                        head = move_to_end(head, p);
                                        head =  move_to_end(head, temp);
					
                          
                                    } else if (re == 0) { // active player finished their turn
                                        display_info(p, p->current);
                                        (p->current)->active = 1;
                                        p->active = 0;
                                        display_moves(p, p->current);
                                        char * wait = malloc(sizeof(char) * 50);
                                        sprintf(wait, "Waiting for %s to strike"
                                                "...\n", p->current->name);
                                        write(p->fd, wait, strlen(wait));
			   	 
                                    } else if (re == 2) { // active player successfully spoke
                                        display_info(p, p->current);  
                                        display_moves(p, p->current);
                                    } else if (re == 4) { // active player exceeded speak limit
				       char buf[100]; 
                                    sprintf(buf, "--%s dropped. You win!\n\n"
                                            "Awaiting next opponent...\r\n", 
                                            p->name);
                                    write((p->current)->fd, buf, strlen(buf));
                                    char drop[50];
                                   
                                 
                                    sprintf(drop, "**%s leaves**\r\n", p->name);
                                   (p->current)->last_battled  = NULL;
                                    (p->current)->current = NULL;
                    
                                    int tmp_fd = p->fd;
                                    head = removeclient(head, p->fd);
                                    FD_CLR(tmp_fd, &allset);
                                    close(tmp_fd);
				     broadcast(head, drop, strlen(drop));
   
                                    (p->current)->last_battled  = NULL;
                                    (p->current)->current = NULL;
				    }
			        }

                            } else if ((p->current != NULL)  // p is in a battle but is not active player
                                        && (p->active == 0)) {
                                char * buffer = malloc(sizeof(char) * 50);
                                int result = read(p->fd, buffer, 
                                                  sizeof(buffer));
                            
                                if (result == 0) { // p dropped
                                    char buf[100];
                                    sprintf(buf, "--%s dropped. You win!\n\n"
                                            "Awaiting next opponent...\r\n", 
                                            p->name);
				    
				    (p->current)->last_battled = NULL; // set p's last battled status to null
                                    write(p->current->fd, buf, strlen(buf));
				    (p->current)->current = NULL;
                                    char drop[50];
                                    sprintf(drop, "**%s leaves**\r\n", p->name);
                                    broadcast(head, drop, strlen(drop));
                                    (p->current)->current =NULL; // p's opponent is not currently combating anyone
				    (p->current)->last_battled = NULL; // p's opponent did last battled no one (since p dropped)
                                    int tmp_fd = p->fd;
                                    head = removeclient(head, p->fd);
                                    FD_CLR(tmp_fd, &allset);
                                    close(tmp_fd);
                                }

		            } else if (p->current == NULL) { // p is not in a battle
		                char * buffer = malloc(sizeof(char) * 50);
		                int result = read(p->fd, buffer,sizeof(buffer));
	                        if (result == 0) {
                                    char drop[50];
                                    sprintf(drop, "**%s leaves**\r\n", p->name);
                                    broadcast(head, drop, strlen(drop));
                                    int tmp_fd = p->fd;
                                    head = removeclient(head, p->fd);
			            FD_CLR(tmp_fd, &allset);
                                    close(tmp_fd);
                                }
		            }
		            break;
                        }	    
                    }
                }
            }
	    // match players 
        struct client *p1;
        struct client  *r  = head;

        for (p1 = head; p1 != NULL; p1 = p1->next) { // go through client list 
	  // if p has entered their name and is not currently in a game, find them a match
	  if ((p1->current == NULL) & (p1->newline_received)) { 
	    if (r != NULL) { 

	      // if r is in a battle, or has not enetered their name, or if r is p or if r last battled p and p last battled r, move on to the client in the list
                    while ((r->current != NULL) | 
			   ( (r->last_battled == p1) & (p1->last_battled == r)) |
                           (p1->fd == r->fd) | (r->newline_received == 0)) {
                        r = r->next;
                        if (r == NULL){ // reached end of client list
                            break;
                        }
                    }
                }
	    if (r != NULL) { // match was found
                // run battle
	        r->current = p1; // r is now currently facing p1
	        p1->current = r; // p1 is now currently facing r
	        initialize_battle(r, p1); // initialize battle between r and p1
                }
            }
        }
    }
    return 0;
}


/* finds the last client in the client list */
struct client * find_last(struct client * top){
   struct client * p = top;
   if (p != NULL) {
       while (p->next != NULL) {
 	p = p->next;
      }
  } 
  return p;
}


/* moves client to the end of client list, updates and returns the head */
struct client *move_to_end(struct client * top, struct client * move) {
    struct client *p = top;
    struct client * head;
    if (move != NULL) {
        if (p == move) { // special case where head is the item we want to move
	    (find_last(top))->next = move; 
	    head = move->next;
            move->next = NULL;
        } else {
            while (p->next != NULL) {
                if (p->next == move) {
                    p->next = move->next;
                    (find_last(top))->next = move;
                    move->next = NULL;
                    head = top;
                    break;
                }
	        p = p->next;
            }
        }
    }
    return head;
}


int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        sprintf(outbuf, "%s says: %s", inet_ntoa(p->ipaddr), buf);
        broadcast(top, outbuf, strlen(outbuf));
        return 0;
    } else if (len == 0) {
        // socket is closed
        //printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "%s leaves\r\n", p->name);
        broadcast(top, outbuf, strlen(outbuf));
        return -1;
    } else { // shouldn't happen
        perror("read");
        return -1;
    }
}


/* bind and listen, abort on error
 * returns FD of listening socket
 */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;
    
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
         &yes, sizeof(int))) == -1) { // sets socket options
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);
    
    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }
    
    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}


/* adds client to client list */
static struct client * addclient(struct client *top, int fd, 
                                 struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
   
    p->name = NULL;
    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    p->last_battled = NULL;
    p->current = NULL;
    p->newline_received = 0;
    p->i =-1;
    p->message = malloc(sizeof(char) * 300);
    top = p;
    return top;
}


/* removes client from client list */
static struct client * removeclient(struct client *top, int fd) {
    struct client **p;
    
    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, "
               "but I don't know about it\n", fd);
    }
    return top;
}


/* writes a message to all clients in client list */
static void broadcast(struct client *top, char *s, int size) {
    struct client *p;   
    for (p = top; p; p = p->next) {
    // broadcast to all clients
       check_for_system_error(write(p->fd, s, size));
    }
    /* should probably check write() return value and perhaps remove client */
}


/* initializes battle between two clients once they have been paired */
void initialize_battle(struct client *p1, struct client *p2) {
    p1->hitpoints = hitpoints_generator();
    p2->hitpoints = hitpoints_generator();
    p1->powermoves = powermoves_generator();
    p2->powermoves = powermoves_generator();
    p1->active = random_number(1);
    p2->active = !(p1->active);
  
    char outbuf1[512];
    char outbuf2[512];

    sprintf(outbuf1, "You engage %s!\r\n", p1->name);
    sprintf(outbuf2, "You engage %s!\r\n", p2->name);
    
    check_for_system_error(write(p2->fd, outbuf1, strlen(outbuf1)));
    check_for_system_error(write(p1->fd, outbuf2, strlen(outbuf2)));
    display_info(p1, p2);
    struct client * active_player;

    // determine who is the active_player
    if (p1->active == 1){
        active_player = p1;
    } else { 
        active_player = p2;
    }

    // display the option menu to the active_player
    char buf[50] = "\n(a)ttack\n(p)owermove\n(s)peak something\r\n";
    check_for_system_error(write(active_player->fd, buf, sizeof(buf)));

}


/* displays hitpoints, powermoves, and opponents moves to both clients that are 
 * in a battle */
void display_info(struct client *p1, struct client * p2) {
    display_hitpoints(p1->hitpoints, p1);
    display_powermoves(p1->powermoves, p1);
    display_opponents_hitpoints(p2->hitpoints, p1, p2);

    display_hitpoints(p2->hitpoints, p2);
    display_powermoves(p2->powermoves, p2);
    display_opponents_hitpoints(p1->hitpoints, p2, p1);
}


/* display_moves prompts the active_player for options that are available
 * for him */
void display_moves(struct client * p1, struct client * p2) {
    struct client * active_player;

    // determine which player is the active player
    if (p1->active == 1) {
      active_player = p1;    
    } else {
      active_player = p2;
    }
    
    // if the active player doesn't have any powermoves left
    if (active_player->powermoves == 0) {

        // only two options are displayed, powermove option is not displayed
        char buf[50] = "\n(a)ttack\n(s)peak something\r\n";
        check_for_system_error(write(active_player->fd, buf , strlen(buf)));
    
    // if the active player still has powermoves
    } else {

        // powermove is an available option for the active player
        char buf[50] = "\n(a)ttack\n(p)owermove\n(s)peak something\r\n";
        check_for_system_error(write(active_player->fd, buf, strlen(buf)));
    } 
}


/* check_hitpoints returns 1 if the nonactive player has lost
 * (nonactive_player->hitpoints<=0), 
 * returns 0 if not
 */
int check_hitpoints(struct client *active_player, 
                    struct client *nonactive_player) {
    char buf[100];
    if (nonactive_player->hitpoints <= 0) {
        sprintf(buf, "%s gives up, You win!\n\nAwaiting next opponent..."
                "\r\n", nonactive_player->name);
        
        // write the message to the active player
        check_for_system_error(write(active_player->fd, buf, strlen(buf)));
        sprintf(buf, "You are no match for %s, You scurry away...\n\n"
                "Awaiting next opponent...\r\n", active_player->name);

        // write the message to the nonactive player
        check_for_system_error(write(nonactive_player->fd, buf, strlen(buf)));
        return 1;
    }
    return 0;
}


/* combat returns 0 if the active player chooses to attack or powermove
 * returns 1 if the nonactive player has lost (hitpoints <= 0),
 * returns 2 if the active player chooses to speak
 * returns 3 if the active player types an unavailable option
 * returns 4 if speak overflows
 */
int combat(struct client * p1, struct client * p2, char *choice) {
    struct client * active_player;
    struct client * nonactive_player;

    // set up active_player and nonactive_player
    if (p1->active == 1) {
        active_player = p1;
        nonactive_player = p2;
    } else {
        active_player = p2;
        nonactive_player = p1;
    }

    char buf[50];

    // the active_player chooses to attack
    if ((strchr(choice, 'a') != NULL) && (active_player->i == -1)) {
        int attack_points = attack_generator();
        sprintf(buf, "\nYou hit %s for %d damage!\r\n",
                nonactive_player->name, attack_points);
        check_for_system_error(write(active_player->fd, buf, strlen(buf)));

        // deducts nonactive_players hitpoints
        nonactive_player->hitpoints = nonactive_player->hitpoints
                                      - attack_points;
        sprintf(buf, "\n%s hits you for %d damage!\r\n",
                active_player->name, attack_points);
        check_for_system_error(write(nonactive_player->fd, buf, strlen(buf)));

        // check if the nonactive_player's hitpoints is <= 0
        if (check_hitpoints(active_player, nonactive_player) == 1) {
            return 1;
        }
        return 0;
    }

    // the active_player chooses to powermove
    if ((strchr(choice, 'p') != NULL) && (active_player->i== -1)){

        // randomly determine if the powermove strikes or misses
        int chance = powerchance_generator();
        int attack_points = attack_generator() * 3;

        // striked
        if (chance == 0) {
            sprintf(buf, "\nYou hit %s for %d damage!\r\n",
                    nonactive_player->name, attack_points);
            check_for_system_error(write(active_player->fd, buf, strlen(buf)));
            sprintf(buf, "%s powermoves you for %d damage!\r\n",
                    nonactive_player->name, attack_points);
            check_for_system_error(write(nonactive_player->fd, 
                                   buf, strlen(buf)));
           
            // updates nonactive player's hitpoints
            nonactive_player->hitpoints = nonactive_player->hitpoints
                                          - attack_points;

        // missed
        } else if (chance == 1) {
            char buf[50] = "\nYou missed!\r\n";
            check_for_system_error(write(active_player->fd, buf, strlen(buf)));
            sprintf(buf, "%s missed you!\r\n", active_player->name);
            check_for_system_error(write(nonactive_player->fd, buf, 
                                   strlen(buf)));
        }
  
        // check if the nonactive_player's hitpoints is <= 0
        if (check_hitpoints(active_player, nonactive_player) == 1) {
            return 1;
        }

        // reduce the active_player's powermoves by 1
        active_player->powermoves --;

        return 0;
    }

    // the active_player chooses to speak
    if ((strchr(choice, 's') != NULL) && active_player->i == -1){
        char wor[2];

        // write the header "Speak" to active_player
        char speak[11] = "\nSpeak: \r\n";
        speak[10] = '\0';
        check_for_system_error(write(active_player->fd, speak, strlen(speak)));
      
        // read the active_player's input one char at a time into words
        int i = 0;
        wor[1] = '\0';
        /*
        if (read(active_player->fd, &(wor[0]), 1)==1){
        if (wor[0] == '\n') {
            return 2;
        } else{
            strcat(&active_player->message[active_player->i], &wor[0]);
            active_player->i ++; 
            return 5;
          }
       } else {
         return 5;
      }
 */ active_player->i ++;
 return 5;
    } else if (active_player->i >= 0) {
       char wor[2];
        char temp[1] = "\0";
        //check_for_system_error(read(active_player->fd, &(wor[0]), 1));
        if ((choice[0] == '\n'))  {
           active_player->message[active_player->i] = '\r';
           active_player->message[active_player->i + 1] = '\n';
           active_player->message[active_player->i +2] = '\0';
         // write the header "takes a break to tell you" to nonactive_player
        char header[60];
        sprintf(header, "%s takes a break to tell you: \r\n", 
                active_player->name);
        header[59] = '\0';
        check_for_system_error(write(nonactive_player->fd, header, strlen(header)));
        write(nonactive_player->fd, active_player->message, strlen(active_player->message)); 
           char end[5] = "\r\n\0";       
           check_for_system_error(write(active_player->fd, end, strlen(end)));

            active_player->i = -1;
           return 2;
        } else if (active_player->i > 296) {
            return 4;
        } else {
            strcat(&active_player->message[active_player->i], &choice[0]);
            active_player->i ++; 
           return 5;
       } 
    }
 /*
        while (((words[i]) != '\n') & (temp[0] != '\n')) {
            if (i < 296) {
            i++;
            check_for_system_error(read(active_player->fd, &(words[i]), 1));
            } else {
                check_for_system_error(read(active_player->fd, temp, 1));
	     
                // active_player exceeded limit of 297 characters
                if ((temp[0] != '\n')) {
                    return 4;
		}
            }
        }
   
        // adds network newline and null terminator 
        words[i+1] = '\r';
        words[i+2] = '\n';
        words[i+3] = '\0';
       
        // write the header "takes a break to tell you" to nonactive_player
        char header[60];
        sprintf(header, "%s takes a break to tell you: \r\n", 
                active_player->name);
        header[59] = '\0';
        check_for_system_error(write(nonactive_player->fd, header, strlen(header)));
    
        // write the words spoken by active_player to nonactive_player
        check_for_system_error(write(nonactive_player->fd, 
                               words, strlen(words)));
 
        char end[5] = "\r\n\0";       
        check_for_system_error(write(active_player->fd, end, strlen(end)));

        return 2;
   */ 

    // invalid option
    return 3;
}


void display_hitpoints(int hitpoints, struct client * p) {
    char buf[50];
    sprintf(buf, "Your hitpoints: %d\r\n", hitpoints);
    check_for_system_error(write(p->fd, buf, strlen(buf)));
}

void display_powermoves(int powermoves, struct client * p) {
    char buf[50];
    sprintf(buf, "Your powermoves: %d\n\r\n", powermoves);
    check_for_system_error(write(p->fd, buf, strlen(buf)));
}

void display_opponents_hitpoints(int hitpoints, struct client * p,
                                 struct client * opponent) {
    char buf[50];
    sprintf(buf, "%s's hitpoints: %d\r\n", opponent->name, hitpoints);
    check_for_system_error(write(p->fd, buf, strlen(buf)));
}


// source: C programming: A Modern Approach
void intialize_number_generator(void) {
    srand((unsigned) time(NULL));
}

int random_number(int max_number) {
    return rand() % max_number + 1;
}

int hitpoints_generator(void) {
    return random_number(10) + 20;
}

int powermoves_generator(void) {
    return random_number(2) + 1;
}

int attack_generator(void) {
    return random_number(4) + 2;
}

// returns 0 means its a hit, returns 1 means its a miss
int powerchance_generator(void) {
    return random_number(1);
}

// helper function to check the return value of system operations
void check_for_system_error (int i) {
    if (i == -1) {
        perror("System operation failed");
        exit(1);
    }
}

// helper function to check the return value of system operations
void check_for_malloc_error (int i) {
    if (i == -1) {
        perror("malloc error");
        exit(1);
    }
}

