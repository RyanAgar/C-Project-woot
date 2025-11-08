/*
  INF1002 C Project
  Group: P9_3 
  Core Features: OPEN, SHOW ALL, SORT, INSERT, QUERY, UPDATE, DELETE, SAVE, SUMMARY
  Unique Features: UNDO + Audit Log
  Log File: P9_3-CMS.log
*/

#include <stdio.h>     // Standard input/output: printf, fgets, fopen, etc.
#include <stdlib.h>    // General utilities: malloc, free, atoi, atof, exit
#include <string.h>    // String handling: strcpy, strcmp, strlen, strtok
#include <time.h>      // Time functions: time, localtime, strftime
#include <ctype.h>     // Character checks: isdigit, toupper, tolower
#include <stdarg.h>    // Variable arguments: va_list, va_start, va_end

#define MAX_STR 128       // Maximum length for strings (e.g. name, programme)
#define INIT_CAP 16       // Initial capacity for student array (used in dynamic resizing)
#define LOGFILE "P9_3-CMS.log"  // Filename for audit logging
#define FILENAME "P9_3-CMS.txt"

// ANSI color codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

int query_exists(int id);  // Checks if a student with the given ID already exists

// Static variable to hold the "current user" for logging
static const char* CURRENT_USER = "P9_3-Admin"; // You can place this just before audit_log

// Structure to store student record
typedef struct {
    int id;                         // Unique student ID
    char name[MAX_STR];            // Student's name
    char programme[MAX_STR];       // Programme enrolled
    float mark;                    // Final mark
} Student;

// Enum to represent the type of last operation (for undo)
typedef enum {OP_NONE, OP_INSERT, OP_DELETE, OP_UPDATE} OpType;

// Structure to store undo information
typedef struct {
    OpType op;                     // Type of last operation
    Student before;               // Record before change
    Student after;                // Record after change
} UndoRecord;

// Dynamic array to store student records
Student *arr = NULL;              // Pointer to student array
size_t arr_size = 0;              // Current number of records
size_t arr_cap = 0;               // Current capacity of array

UndoRecord last_op = {OP_NONE};  // Stores last operation for undo

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
    if(!f) {
        // Print an error message if logging fails
        printf(RED "CMS Error: Failed to open or write to audit log file \"%s\"." RESET "\n", LOGFILE);
        return;
    }

    // 1. Get Timestamp
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

    // 2. Write Contextual Metadata to Log
    // Format
    fprintf(f, "[%s] [%s] (Records: %zu) ", ts, CURRENT_USER, arr_size);

    // 3. Write Variable Arguments
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    // 4. Finalize and Close
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

void print_student_record(const Student *s) {
    const char *color = RESET;
    
    // Decide color based on mark
    if (s->mark >= 80) {
        color = GREEN;    // excellent
    } else if (s->mark < 50) {
        color = RED;      // failing
    } else {
        color = YELLOW;   // average
    }

    printf("%-10d %-20s %-30s %s%-6.1f%s\n",
           s->id,
           s->name,
           s->programme,
           color, s->mark, RESET);
}

/* ---------------------------------------------------- */
/* Robust Parsing (supports tab or variable spacing)    */
/* ---------------------------------------------------- */
int parse_line(const char *line, Student *s) {
    char buf[512];
    strncpy(buf, line, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    // Remove newline
    buf[strcspn(buf, "\r\n")] = 0;

    // Tokenize by whitespace
    char *tokens[64];
    int count = 0;
    char *p = strtok(buf, " \t");
    while (p && count < 64) {
        tokens[count++] = p;
        p = strtok(NULL, " \t");
    }
    if (count < 4) return 0;

    // First token = ID
    s->id = atoi(tokens[0]);

    // Last token = Mark
    s->mark = atof(tokens[count-1]);

    // Next two tokens = First + Last name
    char namebuf[128] = "";
    snprintf(namebuf, sizeof(namebuf), "%s %s", tokens[1], tokens[2]);
    strncpy(s->name, namebuf, MAX_STR-1);
    s->name[MAX_STR-1] = '\0';

    // Everything between surname and mark = Programme
    char progbuf[256] = "";
    for (int i = 3; i < count-1; i++) {
        strcat(progbuf, tokens[i]);
        if (i < count-2) strcat(progbuf, " ");
    }
    strncpy(s->programme, progbuf, MAX_STR-1);
    s->programme[MAX_STR-1] = '\0';

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

void show_all(void) {

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    // Print header in bold cyan
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    // Print each student record with matching widths
    for (size_t i = 0; i < arr_size; i++) {
        const char *color = RESET;

        // Decide color based on mark
        if (arr[i].mark >= 80) {
            color = GREEN;   // excellent
        } else if (arr[i].mark < 50) {
            color = RED;     // failing
        } else {
            color = YELLOW;  // average
        }

        printf("%-10d %-20s %-30s %s%-6.1f%s\n",
               arr[i].id,
               arr[i].name,
               arr[i].programme,
               color, arr[i].mark, RESET);
    }
}


int idAsc(const void* a, const void* b) {
    Student* student_a = (Student*)a;
    Student* student_b = (Student*)b;
    return student_a->id - student_b->id; // access the id and sort in ascending
}

int idDesc(const void* a, const void* b) {
    Student* student_a = (Student*)a;
    Student* student_b = (Student*)b;
    return student_b->id - student_a->id; // access the id and sort in descending
}

int markAsc(const void* a, const void* b) {
    float mark_a = ((Student*)a)->mark;
    float mark_b = ((Student*)b)->mark;
    if (mark_a > mark_b) {
        return 1;  // a is greater so it comes after b
    }
    else if (mark_a < mark_b) {
        return -1; // a is smaller so it comes before b
    }
    else {
        return 0;  // a and b are equal
    }
}

int markDesc(const void* a, const void* b) {
    float mark_a = ((Student*)a)->mark;
    float mark_b = ((Student*)b)->mark;
    if (mark_a > mark_b) {
        return -1;  // a is greater so it comes before b
    }
    else if (mark_a < mark_b) {
        return 1; // a is smaller so it comes after b
    }
    else {
        return 0;  // a and b are equal
    }
}

void showSorted(const char* field, const char* order) {
    if (strcmp(field, "ID") == 0) {
        if (strcmp(order, "ASC") == 0) {
            qsort(arr, arr_size, sizeof(Student), idAsc);
        }
        else if (strcmp(order, "DESC") == 0) {
            qsort(arr, arr_size, sizeof(Student), idDesc);
        }
    }
    else if (strcmp(field, "MARK") == 0) {
        if (strcmp(order, "ASC") == 0) {
            qsort(arr, arr_size, sizeof(Student), markAsc);
        }
        else if (strcmp(order, "DESC") == 0) {
            qsort(arr, arr_size, sizeof(Student), markDesc);
        }
    }
    show_all();
}


void insert_record(Student s){

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }
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

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }
    int i = find_index_by_id(id);
    if (i < 0) {
        printf("CMS: The record with ID %d does not exist.\n", id);
        return;
    }
    // Print header in bold cyan
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    // Print each student record with matching widths
        const char *color = RESET;

        // Decide color based on mark
        if (arr[i].mark >= 80) {
            color = GREEN;   // excellent
        } else if (arr[i].mark < 50) {
            color = RED;     // failing
        } else {
            color = YELLOW;  // average
        }

        printf("%-10d %-20s %-30s %s%-6.1f%s\n",
               arr[i].id,
               arr[i].name,
               arr[i].programme,
               color, arr[i].mark, RESET);
    
}

void update(int id){

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

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

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

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

void save(){

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }
    FILE *f = fopen(FILENAME, "w");
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
    printf("CMS: Saved to \"%s\".\n", FILENAME);
    audit_log("SAVE %s", FILENAME);
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

    printf(CYAN"===== Student Summary =====\n" RESET);
    printf("Total students :  %d\n", total);
    printf("Average mark   :"); 
    printf(YELLOW " % .2f\n" RESET, average);
    printf("Highest mark   : ");
    printf(GREEN "% .1f (% s)\n" RESET, highest, arr[hi_index].name);
    printf("Lowest mark    :");
    printf(RED " % .1f (% s)\n" RESET, lowest, arr[lo_index].name);
    printf(CYAN"===========================\n" RESET);
}

/**
 * @brief Reverts the last INSERT, DELETE, or UPDATE operation.
 */
void undo(){
    if(last_op.op == OP_NONE){
        printf(YELLOW "CMS: Nothing to undo.\n" RESET);
        return;
    }

    printf("\n" BOLD "===== Performing UNDO operation =====" RESET "\n");
    
    // Print the record header for visual clarity
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");
    printf(CYAN "------------------------------------------------------------------\n" RESET);

    if(last_op.op == OP_INSERT){
        // UNDO INSERT: Delete the record that was inserted (stored in last_op.after)
        int i = find_index_by_id(last_op.after.id);
        if(i>=0 && arr_size > 0){
            printf(YELLOW "Removed record:\n" RESET);
            print_student_record(&arr[i]); // Show the record being removed
            
            // Delete the record (swap with last and decrease size)
            arr[i] = arr[arr_size-1];
            arr_size--; 
            
            printf(GREEN "CMS: Undo INSERT successful (Record ID %d removed).\n" RESET, last_op.after.id);
            audit_log("UNDO INSERT (ID %d removed)", last_op.after.id);
        } else {
            printf(RED "CMS Error: Undo failed. Record ID %d not found or array empty.\n" RESET, last_op.after.id);
            audit_log("UNDO INSERT failed (ID %d not found)", last_op.after.id);
        }
    }
    else if(last_op.op == OP_DELETE){
        // UNDO DELETE: Re-insert the deleted record (stored in last_op.before)
        ensure_cap();
        arr[arr_size++] = last_op.before;

        printf(GREEN "Re-inserted record:\n" RESET);
        print_student_record(&last_op.before); // Show the record being re-inserted
        
        printf(GREEN "CMS: Undo DELETE successful (Record ID %d re-inserted).\n" RESET, last_op.before.id);
        audit_log("UNDO DELETE (ID %d re-inserted)", last_op.before.id);
    }
    else if(last_op.op == OP_UPDATE){
        // UNDO UPDATE: Restore the record state from last_op.before
        int i = find_index_by_id(last_op.after.id); // Search using the ID from the 'after' state (current ID)
        if(i>=0) {
            printf(YELLOW "Restoring record from state before update:\n" RESET);
            print_student_record(&last_op.before); // Show the record's state before the update
            
            arr[i] = last_op.before; // Revert the record to the 'before' state
            
            printf(GREEN "CMS: Undo UPDATE successful (Record ID %d reverted).\n" RESET, last_op.before.id);
            audit_log("UNDO UPDATE (ID %d restored)", last_op.before.id);
        } else {
            printf(RED "CMS Error: Undo failed. Record ID %d not found.\n" RESET, last_op.after.id);
            audit_log("UNDO UPDATE failed (ID %d not found)", last_op.after.id);
        }
    }

    printf(BOLD "====================================" RESET "\n");
    last_op.op = OP_NONE; // Clear the UNDO buffer after execution
}

/* ---------------------------------------------------- */
/* Command Loop                                         */
/* ---------------------------------------------------- */

int main(void) {
    char userBuffer[256], command[64], arg1[64], arg2[64], arg3[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    char datetime[64];
    strftime(datetime, sizeof(datetime), "%A, %d %B %Y, %I:%M %p", t);

    printf("Hello there! P9_3 Classroom Management System [CMS] Ready. Today is %s.\n", datetime);
    printf("Type HELP to display available commands.\n");


    while (1) {
        printf("P9_3> ");
        if (!fgets(userBuffer, sizeof(userBuffer), stdin)) break;

        // strip newline from userBuffer
        userBuffer[strcspn(userBuffer, "\n")] = 0;

        // reset command and arguments
        command[0] = arg1[0] = arg2[0] = arg3[0] = '\0';

        // tokenize up to 4 words
        int n = sscanf(userBuffer, "%63s %63s %63s %63s", command, arg1, arg2, arg3);
        if (n < 1) continue;

        if (strcasecmp(command, "OPEN") == 0) {
            if (n >= 2) open_db(arg1);
            else printf("Usage: OPEN filename\n");

        } else if (strcasecmp(command, "SHOW") == 0) {
            if (strcasecmp(arg1, "ALL") == 0) {
                if (strcasecmp(arg2, "SORT") == 0 && strcasecmp(arg3, "BY") == 0) {
                    char field[16] = "\0";
                    char order[16] = "\0";
                    int numOfArgs = sscanf(userBuffer, "%*s %*s %*s %*s %15s %15s", field, order);
                    if (numOfArgs >= 1) {
                        for (int i = 0; field[i] != '\0'; i++) { //change field input to uppercase
                            field[i] = toupper(field[i]);
                        }
                        for (int j = 0; order[j] != '\0'; j++) { //change order input to uppercase
                            order[j] = toupper(order[j]);
                        }
                        if (order[0]) { // order field is provided in input
                            showSorted(field, order);
                        }
                        else (showSorted(field, "ASC"));  // default to asc if order field not specified
                    }
                }
                else {
                    show_all();
                }
            }
            else if (strcasecmp(arg1, "SUMMARY") == 0) {
                summary();
            }
            else {
                printf("Usage: SHOW ALL | SHOW SUMMARY | SHOW ALL SORT BY ...\n");
        }


        } else if (strcasecmp(command, "INSERT") == 0) {
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

        } else if (strcasecmp(command, "QUERY") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            query(id);
            } else {
            printf("Usage: QUERY <ID>\n");
            }
        } else if (strcasecmp(command, "UPDATE") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            update(id);
            } else {
            printf("Usage: UPDATE <ID>\n");
            }

        } else if (strcasecmp(command, "DELETE") == 0) {
            if (n >= 2) {
            int id = atoi(arg1);
            delete(id);
            } else {
            printf("Usage: DELETE <ID>\n");
            }

        } else if (strcasecmp(command, "SAVE") == 0) {
            if (n >= 1) save();
            else printf("Usage: SAVE\n");

        } else if (strcasecmp(command, "UNDO") == 0) {
            undo();

        } else if (strcasecmp(command, "HELP") == 0) {
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

        } else if (strcasecmp(command, "EXIT") == 0) {
            break;

        } else {
            printf("Unknown command.\n");
        }
    }

    free(arr);
    return 0;
}


