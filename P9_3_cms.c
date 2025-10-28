/*
  INF1002 C Project
  Group: P9_3 
  Core Features: OPEN, SHOW ALL, SORT, INSERT, QUERY, UPDATE, DELETE, SAVE, SUMMARY
  Unique Features: UNDO + Audit Log
  Log File: P9_3-CMS.log
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#define MAX_STR 128
#define INIT_CAP 16
#define LOGFILE "P9_3-CMS.log"

int query_exists(int id);

typedef struct {
    int id;
    char name[MAX_STR];
    char programme[MAX_STR];
    float mark;
} Student;

typedef enum {OP_NONE, OP_INSERT, OP_DELETE, OP_UPDATE} OpType;
typedef struct {
    OpType op;
    Student before;
    Student after;
} UndoRecord;

Student *arr = NULL;
size_t arr_size = 0;
size_t arr_cap = 0;

UndoRecord last_op = {OP_NONE};

/* ---------------------------------------------------- */
/* Utility Functions                                    */
/* ---------------------------------------------------- */

int query_exists(int id) {  //Student ID Check
    for (size_t i = 0; i < arr_size; i++) {
        if (arr[i].id == id) {
            return 1; // found
        }
    }
    return 0; // not found
}


void audit_log(const char *fmt, ...) {
    FILE *f = fopen(LOGFILE, "a");
    if(!f) return;

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(f, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    fprintf(f, "\n");
    fclose(f);
}

void ensure_cap() {
    if(arr_cap == 0){
        arr_cap = INIT_CAP;
        arr = malloc(arr_cap * sizeof(Student));
    } else if(arr_size >= arr_cap){
        arr_cap *= 2;
        arr = realloc(arr, arr_cap * sizeof(Student));
    }
}

int find_index_by_id(int id){
    for(size_t i=0;i<arr_size;i++)
        if(arr[i].id == id) return (int)i;
    return -1;
}

/* ---------------------------------------------------- */
/* Robust Parsing (supports tab or variable spacing)    */
/* ---------------------------------------------------- */
int parse_line(const char *line, Student *s){
    char buf[512];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    /* Remove newline */
    buf[strcspn(buf, "\r\n")] = 0;

    /* If TABs exist, split directly — safest */
    if(strchr(buf, '\t')){
        char *id = strtok(buf, "\t");
        char *name = strtok(NULL, "\t");
        char *prog = strtok(NULL, "\t");
        char *mark = strtok(NULL, "\t");
        if(!id || !name || !prog || !mark) return 0;
        s->id = atoi(id);
        strncpy(s->name, name, MAX_STR-1);
        strncpy(s->programme, prog, MAX_STR-1);
        s->mark = atof(mark);
        return 1;
    }

    /* Fallback: variable token parsing */
    char *tokens[64];
    int count = 0;
    char *p = strtok(buf, " ");
    while(p && count < 64){
        tokens[count++] = p;
        p = strtok(NULL, " ");
    }
    if(count < 4) return 0;

    s->id = atoi(tokens[0]);
    s->mark = atof(tokens[count-1]);

    /* Middle tokens are name + programme. We guess split by assuming:
       If name may vary, we let user clarify later when needed.
       But since data already aligned in sample and lab, we combine middle tokens,
       then allow editing after loading if needed.
    */
    char mid[256] = "";
    for(int i=1;i<count-1;i++){
        strcat(mid, tokens[i]);
        if(i < count-2) strcat(mid, " ");
    }

    strncpy(s->name, mid, MAX_STR-1);
    s->programme[0] = '\0'; /* Editable via UPDATE if refinement needed */

    return 1;
}

/* ---------------------------------------------------- */
/* Core Operations                                      */
/* ---------------------------------------------------- */

int open_db(const char *filename){
    FILE *f = fopen(filename, "r");
    if(!f){
        printf("CMS: Failed to open \"%s\"\n", filename);
        return 0;
    }
    arr_size = 0;
    char line[512];
    int line_no = 0;

    while(fgets(line, sizeof(line), f)){
        line_no++;
        if (line_no <= 5) continue; // skip metadata + header

        Student s;
        if(parse_line(line, &s)){
            ensure_cap();
            arr[arr_size++] = s;
        }
    }
    fclose(f);
    printf("CMS: \"%s\" opened (%zu records)\n", filename, arr_size);
    audit_log("OPEN %s (%zu records)", filename, arr_size);
    last_op.op = OP_NONE;
    return 1;
}

void show_all() {
    // Print header with fixed widths
    printf("%-10s %-15s %-25s %-6s\n", "ID", "Name", "Programme", "Mark");

    // Print each student record with matching widths
    for (size_t i = 0; i < arr_size; i++) {
        printf("%-10d %-15s %-25s %-6.1f\n",
               arr[i].id,
               arr[i].name,
               arr[i].programme,
               arr[i].mark);
    }
}


int cmp_id_asc(const void *a, const void *b){ return ((Student*)a)->id - ((Student*)b)->id; }
int cmp_id_desc(const void *a, const void *b){ return cmp_id_asc(b,a); }
int cmp_mark_asc(const void *a, const void *b){
    float x=((Student*)a)->mark, y=((Student*)b)->mark;
    return (x>y)-(x<y);
}
int cmp_mark_desc(const void *a, const void *b){ return cmp_mark_asc(b,a); }

void sort_and_show(const char *field, const char *order){
    if(strcmp(field,"ID")==0){
        qsort(arr, arr_size, sizeof(Student),
             (strcmp(order,"ASC")==0?cmp_id_asc:cmp_id_desc));
    } else if(strcmp(field,"MARK")==0){
        qsort(arr, arr_size, sizeof(Student),
             (strcmp(order,"ASC")==0?cmp_mark_asc:cmp_mark_desc));
    }
    show_all();
}

void insert_record(Student s){
    if(find_index_by_id(s.id) != -1){
        printf("CMS: ID already exists!\n");
        return;
    }
    ensure_cap();
    arr[arr_size++] = s;
    printf("CMS: Record inserted successfully!\n");
    audit_log("INSERT %d %s %s %.1f", s.id, s.name, s.programme, s.mark);
    last_op.op = OP_INSERT;
    last_op.after = s;
}

void query(int id){
    int i = find_index_by_id(id);
    if (i < 0) {
        printf("CMS: The record with ID %d does not exist.\n", id);
        return;
    }

    printf("%d\t%s\t%s\t%.1f\n", arr[i].id, arr[i].name, arr[i].programme, arr[i].mark);
}

void update(int id){
    int i = find_index_by_id(id);
    if(i<0){ printf("CMS: The record with ID %d does not exist.\n"); return; }

    Student before = arr[i], after = before;
    char buf[256];

    printf("New Name (enter to keep \"%s\"): ", before.name);
    fgets(buf, sizeof(buf), stdin);
    if(buf[0] != '\n') strncpy(after.name, strtok(buf, "\r\n"), MAX_STR-1);

    printf("New Programme (enter to keep \"%s\"): ", before.programme);
    fgets(buf, sizeof(buf), stdin);
    if(buf[0] != '\n') strncpy(after.programme, strtok(buf, "\r\n"), MAX_STR-1);

    printf("New Mark (enter to keep %.1f): ", before.mark);
    fgets(buf, sizeof(buf), stdin);
    if(buf[0] != '\n') after.mark = atof(buf);

    arr[i] = after;
    printf("CMS: Record updated.\n");
    audit_log("UPDATE %d", id);
    last_op.op = OP_UPDATE;
    last_op.before = before;
    last_op.after = after;
}

void delete(int id){
    int i = find_index_by_id(id);
    if(i<0){ printf("CMS: The record with ID %d does not exist.\n"); return; }

    char confirm[8];
    printf("Confirm delete (Y/N)? ");
    fgets(confirm, sizeof(confirm), stdin);
    if(toupper(confirm[0])!='Y'){ printf("Cancelled.\n"); return; }

    Student before = arr[i];
    arr[i] = arr[arr_size-1];
    arr_size--;

    printf("CMS: Record deleted.\n");
    audit_log("DELETE %d", before.id);
    last_op.op = OP_DELETE;
    last_op.before = before;
}

void save(const char *filename){
    FILE *f = fopen(filename, "w");
    if(!f){ printf("Save failed.\n"); return; }

    // Write metadata header
    fprintf(f, "Database Name: P9_3-CMS\n");
    fprintf(f, "Authors: Ryan, Glenn, Min Han, Jordan, Ben\n");
    fprintf(f, "Table Name: StudentRecords\n\n");

    // Column headers
    fprintf(f, "%-10s %-15s %-25s %-6s\n", "ID", "Name", "Programme", "Mark");

    // Write student records
    for(size_t i=0;i<arr_size;i++)
        fprintf(f, "%-10d %-15s %-25s %-6.1f\n", arr[i].id, arr[i].name, arr[i].programme, arr[i].mark);

    fclose(f);
    printf("CMS: Saved to \"%s\".\n", filename);
    audit_log("SAVE %s", filename);
}

void summary() {
    if (arr_size == 0) {
        printf("No students available.\n");
        return;
    }

    int total = arr_size;
    double sum = 0.0;
    double highest = arr[0].mark;
    double lowest = arr[0].mark;
    size_t hi_index = 0;
    size_t lo_index = 0;

    for (size_t i = 0; i < arr_size; i++) {
        double m = arr[i].mark;
        sum += m;

        if (m > highest) {
            highest = m;
            hi_index = i;
        }
        if (m < lowest) {
            lowest = m;
            lo_index = i;
        }
    }

    double average = sum / total;

    printf("===== Student Summary =====\n");
    printf("Total students : %d\n", total);
    printf("Average mark   : %.2f\n", average);
    printf("Highest mark   : %.1f (%s)\n", highest, arr[hi_index].name);
    printf("Lowest mark    : %.1f (%s)\n", lowest, arr[lo_index].name);
    printf("===========================\n");
}

void undo(){
    if(last_op.op == OP_NONE){
        printf("Nothing to undo.\n");
        return;
    }

    if(last_op.op == OP_INSERT){
        int i = find_index_by_id(last_op.after.id);
        if(i>=0){ arr[i] = arr[arr_size-1]; arr_size--; }
        printf("Undo INSERT done.\n");
    }
    else if(last_op.op == OP_DELETE){
        ensure_cap();
        arr[arr_size++] = last_op.before;
        printf("Undo DELETE done.\n");
    }
    else if(last_op.op == OP_UPDATE){
        int i = find_index_by_id(last_op.after.id);
        if(i>=0) arr[i] = last_op.before;
        printf("Undo UPDATE done.\n");
    }

    audit_log("UNDO");
    last_op.op = OP_NONE;
}

/* ---------------------------------------------------- */
/* Command Loop                                         */
/* ---------------------------------------------------- */

int main(void) {
    char line[256], cmd[64], arg1[64], arg2[64], arg3[64];

    printf("P9_3 CMS Ready. Type HELP.\n");

    while (1) {
        printf("P9_3> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        // strip newline
        line[strcspn(line, "\n")] = 0;

        // reset args
        cmd[0] = arg1[0] = arg2[0] = arg3[0] = '\0';

        // tokenize up to 4 words
        int n = sscanf(line, "%63s %63s %63s %63s", cmd, arg1, arg2, arg3);
        if (n < 1) continue;

        if (strcasecmp(cmd, "OPEN") == 0) {
            if (n >= 2) open_db(arg1);
            else printf("Usage: OPEN filename\n");

        } else if (strcasecmp(cmd, "SHOW") == 0) {
            if (strcasecmp(arg1, "ALL") == 0) {
                if (strcasecmp(arg2, "SORT") == 0 && strcasecmp(arg3, "BY") == 0) {
                    char field[16], order[16];
                    if (sscanf(line, "%*s %*s %*s %*s %15s %15s", field, order) >= 1) {
                        for (char *p = field; *p; p++) *p = toupper(*p);
                        for (char *p = order; *p; p++) *p = toupper(*p);
                        sort_and_show(field, (order[0] ? order : "ASC"));
                    }
                } else {
                    show_all();
                }
            } else if (strcasecmp(arg1, "SUMMARY") == 0) {
                summary();
            } else {
                printf("Usage: SHOW ALL | SHOW SUMMARY | SHOW ALL SORT BY ...\n");
            }

        } else if (strcasecmp(cmd, "INSERT") == 0) {
            Student s;
            char buf[256];

            // Prompt for ID first
            printf("ID: ");
            if (!fgets(buf, sizeof(buf), stdin)) continue;
            s.id = atoi(buf);

            // Check if ID already exists
            if (query_exists(s.id)) {   // <-- implement this helper
                printf("Error: Student with ID %d already exists.\n", s.id);
                continue; // abort insert early
            }

            // Only ask for the rest if ID is unique
            printf("Name: ");
            fgets(buf, sizeof(buf), stdin);
            strtok(buf, "\n");
            strncpy(s.name, buf, MAX_STR-1);

            printf("Programme: ");
            fgets(buf, sizeof(buf), stdin);
            strtok(buf, "\n");
            strncpy(s.programme, buf, MAX_STR-1);

            printf("Mark: ");
            fgets(buf, sizeof(buf), stdin);
            s.mark = atof(buf);

            insert_record(s);

        } else if (strcasecmp(cmd, "QUERY") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            query(id);
            } else {
            printf("Usage: QUERY <ID>\n");
            }
        } else if (strcasecmp(cmd, "UPDATE") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            query(id);
            } else {
            printf("Usage: UPDATE <ID>\n");
            }

        } else if (strcasecmp(cmd, "DELETE") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            query(id);
            } else {
            printf("Usage: DELETE <ID>\n");
            }

        } else if (strcasecmp(cmd, "SAVE") == 0) {
            if (n >= 2) save(arg1);
            else printf("Usage: SAVE\n");

        } else if (strcasecmp(cmd, "UNDO") == 0) {
            undo();

        } else if (strcasecmp(cmd, "HELP") == 0) {
            printf("Commands:\n"
                   "OPEN <file>\n"
                   "SHOW ALL\n"
                   "SHOW ALL SORT BY ID|MARK ASC|DESC\n"
                   "SHOW SUMMARY\n"
                   "INSERT\n"
                   "QUERY <ID>\n"
                   "UPDATE <ID>\n"
                   "DELETE <ID>\n"
                   "SAVE\n"
                   "UNDO\n"
                   "EXIT\n");

        } else if (strcasecmp(cmd, "EXIT") == 0) {
            break;

        } else {
            printf("Unknown command.\n");
        }
    }

    free(arr);
    return 0;
}

