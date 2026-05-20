#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define TEAMS 3
#define RUNNERS_PER_TEAM 5
#define TARGET_SCORE 30

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Usage: relay_race <favoritism_coefficient>\n");
        exit(1);
    }

    int favoritism = atoi(argv[1]);
    printf("--- Starting Relay Race with Favoritism: %d%% ---\n", favoritism);

    int baton_lock = israeli_create(favoritism);
    if(baton_lock < 0) {
        printf("Error: Failed to create baton lock.\n");
        exit(1);
    }

    int score_pipes[TEAMS][2];
    for (int i = 0; i < TEAMS; i++) {
        if(pipe(score_pipes[i]) < 0) {
            printf("Error creating pipe.\n");
            exit(1);
        }
        int initial_score = 0;
        write(score_pipes[i][1], &initial_score, sizeof(int));
    }


    for (int i = 0; i < TEAMS * RUNNERS_PER_TEAM; i++) {
        int pid = fork();
        if (pid == 0) {
            int team_id = i % TEAMS; 
            setgid(team_id);
            
            while(1) {
                israeli_acquire(baton_lock);
                
                int current_score;
                read(score_pipes[team_id][0], &current_score, sizeof(int));
                
                if (current_score >= TARGET_SCORE) {
                    write(score_pipes[team_id][1], &current_score, sizeof(int)); // החזרת הניקוד
                    israeli_release(baton_lock);
                    exit(0);
                }

                current_score++;
                

                printf("Runner %d (Team %d) acquired the baton\n", getpid(), team_id);
                printf("Team %d score = %d\n", team_id, current_score);
                
                write(score_pipes[team_id][1], &current_score, sizeof(int));
                
                if (current_score >= TARGET_SCORE) {
                    printf("\n>>> TEAM %d WINS THE RACE! <<<\n\n", team_id);
                }
                
                israeli_release(baton_lock);
                
                if (current_score >= TARGET_SCORE) {
                    exit(0); 
                }
                
                sleep(2);
            }
        }
    }


    for (int i = 0; i < TEAMS * RUNNERS_PER_TEAM; i++) {
        wait(0);
    }


    israeli_destroy(baton_lock);
    for (int i = 0; i < TEAMS; i++) {
        close(score_pipes[i][0]);
        close(score_pipes[i][1]);
    }

    printf("--- Race Finished ---\n");
    exit(0);
}