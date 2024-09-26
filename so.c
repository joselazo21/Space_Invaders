#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define MAX_ENEMIES 5
#define INITIAL_LIVES 5
#define ENEMY_SPEED 500000

#define NUM_QUEUES 3
#define QUANTUM 10


int score = 0; 
int score_show=0;

int enemy_speed = ENEMY_SPEED;  
int score_threshold = 100;      
int speed_increment = 200000;    

int spaceship_x;
int max_x, max_y;
int lives = INITIAL_LIVES;
pthread_mutex_t lock;
int enemy_count = 0;



//struct definitions

typedef struct Task {
    void (*task_func)(struct Task*);
    int current_queue;
    int time_remaining;
    void *data; 
} Task;

typedef struct Bullet {
    int x, y;
    int active;
    struct Bullet* next;
} Bullet;

typedef struct Enemy {
    int x, y;
    int active;
    struct Enemy* next;
} Enemy;

Bullet* bullets = NULL;
Enemy* enemies = NULL;

Task task_list[NUM_QUEUES * 3]; 
Task* queues[NUM_QUEUES][NUM_QUEUES];
int queue_sizes[NUM_QUEUES] = {0, 0, 0};


int get_enemy_width() {
    return 5; // Width of UFO enemy
}

//drawing functions
void draw_bullet(Bullet *bullet) {
    if (bullet->active) {
        mvprintw(bullet->y, bullet->x, "|");
    }
}

void draw_enemy(Enemy *enemy) {
    if (enemy->active) {
        mvprintw(enemy->y, enemy->x - 2, "<=o=>");
    }
}

//functions that generates an object

void shoot_bullet() {
    Bullet *new_bullet = (Bullet *)malloc(sizeof(Bullet));
    if (new_bullet == NULL) {
        perror("Error al asignar memoria para la bala");
        exit(EXIT_FAILURE);
    }
    new_bullet->x = spaceship_x + 1;
    new_bullet->y = max_y - 3;
    new_bullet->active = 1;
    new_bullet->next = bullets;
    bullets = new_bullet;
}

void generate_enemy() {
    if (enemy_count < MAX_ENEMIES) {  // Sets a limit to the number of enemies on screen
        Enemy *new_enemy = (Enemy *)malloc(sizeof(Enemy));
        if (new_enemy == NULL) {
            perror("Error al asignar memoria para el enemigo");
            exit(EXIT_FAILURE);
        }
        new_enemy->x = rand() % (max_x - 5) + 2;  
        new_enemy->y = 0;
        new_enemy->active = 1;
        new_enemy->next = enemies;
        enemies = new_enemy;
        enemy_count++;  
    }
}

//update functions

void update_bullet(Task *task) {
    Bullet* bullet = (Bullet *)(task->data); 
    if (bullet->active) {
        bullet->y--;    // The bullet moves upwards
        if (bullet->y < 0) {  // In case the bullet moves out the screen
            bullet->active = 0;  // Active will be set on 0
        }
    }
}

void update_enemy(Task *task) {
    Enemy* enemy = (Enemy *)(task->data); 
    if (enemy->active) {
        enemy->y++;    // The enemy moves downwards
        if (enemy->y > max_y) {  // If the enemy moves out the screen
            enemy->active = 0;  // Deactivate the enemy
            enemy_count--;  // 
            lives--;  // Reduce a life
            if (lives <= 0) {  // If the player runs out of lifes
                clear();
                mvprintw(max_y / 2, (max_x - 40) / 2, "GAME OVER");//GAME OVER
                refresh();
                sleep(2);
                endwin();
                printf("Game Over!\n");
                exit(0);
            }
        }
    }
}

void update_enemy_speed() {
    // Checks if the player make another 100 points
    if (score == score_threshold ) {
        
        enemy_speed -= speed_increment ;
        if (enemy_speed < 100000) {  // Sets a limit to speed
            enemy_speed = 100000;
        }
        score = 0 ;  
    }
}

//checking if user make it to kill an enemy

void check_collisions(Task* task) {
    Bullet *curr_bullet = bullets;
    while (curr_bullet != NULL) {
        if (curr_bullet->active) {
            Enemy *curr_enemy = enemies;
            while (curr_enemy != NULL) {
                if (curr_enemy->active) {
                    int enemy_width = get_enemy_width();
                    if (curr_bullet->y >= curr_enemy->y - 1 && curr_bullet->y <= curr_enemy->y + 1) {
                        if (curr_bullet->x >= curr_enemy->x - 2 && curr_bullet->x < curr_enemy->x - 2 + enemy_width) {
                            curr_bullet->active = 0;
                            curr_enemy->active = 0;
                            score_show += 10;
                            score+=10;
                            enemy_count--;
                            break;  
                        }
                    }
                }
                curr_enemy = curr_enemy->next;
            }
        }
        curr_bullet = curr_bullet->next;
    }
}

//wrappers

void update_bullet_wrapper(Bullet* bullet) {
    Task temp_task;
    temp_task.task_func = update_bullet;
    temp_task.current_queue = 0;
    temp_task.time_remaining = 0;
    temp_task.data = bullet; 

    update_bullet(&temp_task);
}

void update_enemy_wrapper(Enemy* enemy) {
    Task temp_task;
    temp_task.task_func = update_enemy;
    temp_task.current_queue = 0;
    temp_task.time_remaining = 0;
    temp_task.data = enemy;  

    update_enemy(&temp_task);
}

void check_collisions_wrapper() {
    Task temp_task;
    temp_task.task_func = check_collisions;
    temp_task.current_queue = 0;
    temp_task.time_remaining = 0;
    temp_task.data = NULL; 

    check_collisions(&temp_task);
}



//functions that free memory spaces

void free_inactive_bullets() {
    Bullet *curr = bullets;
    Bullet *prev = NULL;
    while (curr != NULL) {
        if (!curr->active) {
            Bullet *temp = curr;
            if (prev == NULL) {
                bullets = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr = curr->next;
            free(temp);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void free_inactive_enemies() {
    Enemy *curr = enemies;
    Enemy *prev = NULL;
    while (curr != NULL) {
        if (!curr->active) {
            Enemy *temp = curr;
            if (prev == NULL) {
                enemies = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr = curr->next;
            free(temp);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

//Initializing our tasks

void initialize_tasks() {

    // Initialization of the bullet update task
    Task *bullet_task;
    Bullet *current_bullet = bullets;
    int i = 0;
    while (current_bullet != NULL && i < NUM_QUEUES * 3) {
        bullet_task = &task_list[i++];
        bullet_task->task_func = update_bullet;
        bullet_task->current_queue = 0;
        bullet_task->time_remaining = QUANTUM;
        
        queues[0][queue_sizes[0]++] = bullet_task;
        current_bullet = current_bullet->next;
    }

    // Initalization of the enemy update task
    Task *enemy_task;
    Enemy *current_enemy = enemies;
    while (current_enemy != NULL && i < NUM_QUEUES * 3) {
        enemy_task = &task_list[i++];
        enemy_task->task_func = update_enemy;
        enemy_task->current_queue = 0;
        enemy_task->time_remaining = QUANTUM;
        
        queues[0][queue_sizes[0]++] = enemy_task;
        current_enemy = current_enemy->next;
    }

    // Initalization of the check collisions task
    if (i < NUM_QUEUES * 3) {
    Task *collision_task = &task_list[i];
    collision_task->task_func = check_collisions;
    collision_task->current_queue = 0;
    collision_task->time_remaining = QUANTUM;
    collision_task->data = NULL;  
    
    queues[0][queue_sizes[0]++] = collision_task;
}
}


//Scheduler

void mlfq_scheduler() {
    // Iterates over all queues
    for (int q = 0; q < NUM_QUEUES; q++) {
        // As long as there are tasks in queues
        int t = 0;
        while (t < queue_sizes[q]) {
            Task *current_task = queues[q][t];
            
            // Execute the task if it has remaining time
            if (current_task->time_remaining > 0) {
                // Execution of the current task
                current_task->task_func(current_task);
                
                // If remaining time is greater than quantum, reduce it by quantum
                if (current_task->time_remaining > QUANTUM) {
                    current_task->time_remaining -= QUANTUM;
                } else {
                    // If remaining time is less than or equal to quantum, set it to 1
                    current_task->time_remaining = 1;
                }

                // Move the task to the next lower priority queue
                if (current_task->time_remaining > 1) {
                    current_task->current_queue = (current_task->current_queue + 1) % NUM_QUEUES;

                    for (int k = t; k < queue_sizes[q] - 1; k++) 
                    {
                      queues[q][k] = queues[q][k + 1];
                    }
                    queue_sizes[q]--;

                    // Add the task to the lower priority queue
                    int new_queue = current_task->current_queue;
                    queues[new_queue][queue_sizes[new_queue]++] = current_task;
                } 
                else {
                    // If the task has finished, keep it in the same queue
                    current_task->current_queue = current_task->current_queue;
                }

                
            }
            t++;
        }
    }
}


//thread functions

void* bullet_generation_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        shoot_bullet();  // Shoots a bullet automatically 
        pthread_mutex_unlock(&lock);
        sleep(1);  // Shoot lapse
    }
    return NULL;
}

void* bullet_update_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        Bullet* curr = bullets;
        while (curr != NULL) {
            update_bullet_wrapper(curr);
            curr = curr->next;
        }
        check_collisions_wrapper();
        free_inactive_bullets();  // Frees memory from inactive bullets
        pthread_mutex_unlock(&lock);
        usleep(30000);
    }
    return NULL;
}


void* input_thread(void* arg) {
    while (1) {
        int ch = getch();  // Wait for the user input
        pthread_mutex_lock(&lock);
        switch (ch) {
            case KEY_LEFT:
                if (spaceship_x > 0)
                    spaceship_x--;  // Moves leftwards
                break;
            case KEY_RIGHT:
                if (spaceship_x < max_x - 3)
                    spaceship_x++;  // Moves rightwards
                break;
            case 'q':  // Exit the program
                endwin();  // End up the ncurses mode
                pthread_mutex_unlock(&lock);
                pthread_exit(NULL);
                exit(0);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* enemy_thread(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        if (enemy_count < MAX_ENEMIES) {
            generate_enemy();
        }
        Enemy* curr = enemies;
        while (curr != NULL) {
            update_enemy_wrapper(curr);
            curr = curr->next;
        }
        pthread_mutex_unlock(&lock);
        free_inactive_enemies();
        usleep(enemy_speed);
    }
    return NULL;
}

//Start menu

void show_start_menu() {
    int choice = 0;
    int ch;

    while (1) {
        clear();  // Clear the screen
        mvprintw(max_y / 2 - 1, (max_x - 4) / 2, "Jugar");
        mvprintw(max_y / 2 + 1, (max_x - 4) / 2, "Salir");

        // Highlight the current choice
        if (choice == 0) {
            mvprintw(max_y / 2 - 1, (max_x - 4) / 2 - 2, "> Jugar <");
        } else {
            mvprintw(max_y / 2 + 1, (max_x - 4) / 2 - 2, "> Salir <");
        }

        refresh();  // Refresh to show changes

        ch = getch();

        switch (ch) {
            case KEY_UP:
                choice = (choice - 1 + 2) % 2;
                break;
            case KEY_DOWN:
                choice = (choice + 1) % 2;
                break;
            case '\n':  // Enter key
                if (choice == 0) {
                    return;  // Start the game
                } else {
                    endwin();  // End ncurses mode
                    exit(0);  // Exit the program
                }
        }
    }
}

//Free memory once the game has finished

void cleanup() {
    Bullet *curr_bullet = bullets;
    while (curr_bullet != NULL) {
        Bullet *temp = curr_bullet;
        curr_bullet = curr_bullet->next;
        free(temp);
    }

    Enemy *curr_enemy = enemies;
    while (curr_enemy != NULL) {
        Enemy *temp = curr_enemy;
        curr_enemy = curr_enemy->next;
        free(temp);
    }
}

//MAIN

int main() {
    srand(time(NULL));

    initscr();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);
    
    pthread_mutex_init(&lock, NULL);
    getmaxyx(stdscr, max_y, max_x);
    spaceship_x = max_x / 2;

    show_start_menu();

    initialize_tasks();

    pthread_t bullet_gen_tid, bullet_upd_tid, input_tid, enemy_tid;
    pthread_create(&bullet_gen_tid, NULL, bullet_generation_thread, NULL);
    pthread_create(&bullet_upd_tid, NULL, bullet_update_thread, NULL);
    pthread_create(&input_tid, NULL, input_thread, NULL);
    pthread_create(&enemy_tid, NULL, enemy_thread, NULL);

    while (1) {
        pthread_mutex_lock(&lock);
        clear();
        
        mvprintw(max_y - 1, spaceship_x, " * ");
        mvprintw(max_y - 2, spaceship_x, "/ \\");

        Bullet* curr_bullet = bullets;
        while (curr_bullet != NULL) {
            draw_bullet(curr_bullet);
            curr_bullet = curr_bullet->next;
        }

        Enemy* curr_enemy = enemies;
        while (curr_enemy != NULL) {
            draw_enemy(curr_enemy);
            curr_enemy = curr_enemy->next;
        }

        mvprintw(0, 0, "Lives: %d", lives);
        mvprintw(2, 0, "Score: %d", score_show);

        refresh();
        pthread_mutex_unlock(&lock);
        
        update_enemy_speed();
        mlfq_scheduler();
        usleep(30000);  
    }

    endwin();
    pthread_mutex_destroy(&lock);
    cleanup();  

    return 0;

}