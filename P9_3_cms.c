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

//ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

#define MAX_STR 128 //Maximum length for strings (e.g. name, programme)
#define INIT_CAP 16 //Initial capacity for student array (can be resized)
#define LOGFILE "P9_3-CMS.log" //Default Filename for audit log
#define FILENAME "P9_3-CMS.txt" //Default Filename for student record database
static const char* CURRENT_USER = "P9_3-Admin"; //For audit logging (current user)


//Student Object
typedef struct {
    int id;                  //Student ID (must be unique)
    char name[MAX_STR];      //Student's Name
    char programme[MAX_STR]; //Programme enrolled
    float mark;              //Final marks
} Student;

Student *arr = NULL; //Student Object Array
size_t arr_size = 0; //Student record number tracker
size_t arr_cap = 0; //Array capacity tracker


//Last Operation EnumType (for undo)
typedef enum {
    OP_NONE,   //No operation
    OP_INSERT, //Insert operation
    OP_DELETE, //Delete operation
    OP_UPDATE  //Update operation
} OpType;

//Undo Object
typedef struct {
    OpType op;      //Type of last operation
    Student before; //Student record before change
    Student after;  //Student record after change
} UndoRecord;
UndoRecord last_op = {OP_NONE}; //Initialise last_op


/* ---------------------------------------------------- */
/* Utility Functions                                    */
/* ---------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: query_exists
// PURPOSE : Checks if a given student ID is already present in the array.
// RETURNS : 1 -> ID found
//           0 -> ID not found
// -----------------------------------------------------------------------------
int query_exists(int id) {
    for (size_t i = 0; i < arr_size; i++){
        if (arr[i].id == id) {
            return 1; //ID matched
        }
    }
    return 0; //ID not found
}

// -----------------------------------------------------------------------------
// FUNCTION: audit_log
// PURPOSE : Writes an entry to the audit log file with:
//           - timestamp
//           - username
//           - current record count
//           - custom log message
// ACCEPTS : printf-style format string + variable arguments (...)
// -----------------------------------------------------------------------------
void audit_log(const char *formatString, ...) {
    FILE *logFilePointer = fopen(LOGFILE, "a"); //append mode

    if (!logFilePointer) { //NULL file
        printf(RED "CMS Error: Failed to open or write to audit log file \"%s\"." RESET "\n", LOGFILE);
        return;
    }

    //Generate Current Timestamp
    time_t currentRawTime = time(NULL); //Current time (unix epoch)
    struct tm *tm = localtime(&currentRawTime); //Convert to local timezone
    char timeStamp[32];
    strftime(timeStamp, sizeof(timeStamp), "%Y-%m-%d %H:%M:%S", tm); //Readable format

    //Log prefix
    //Example:
    //[2025-02-01 10:12:34] [P9_3-Admin] (Records: 12)
    fprintf(logFilePointer, "[%s] [%s] (Records: %zu) ", timeStamp, CURRENT_USER, arr_size);

    //Handle variable arguments
    va_list argList;
    va_start(argList, formatString);
    vfprintf(logFilePointer, formatString, argList);  //Append formatted message into audit log
    fprintf(logFilePointer, "\n");
    va_end(argList);

    fclose(logFilePointer); //Close file
}

// -----------------------------------------------------------------------------
// FUNCTION: ensure_cap
// PURPOSE : Ensures student array has enough memory capacity to store new records
// -----------------------------------------------------------------------------
void ensure_cap() {
    //Allocate initial buffer
    if (arr_cap == 0) {
        arr_cap = INIT_CAP;
        arr = malloc(arr_cap * sizeof(Student));
    }
    // If student array is full (size >= capacity), expand capacity
    else if (arr_size >= arr_cap) {
        arr_cap *= 2; //Double capacity
        arr = realloc(arr, arr_cap * sizeof(Student)); //Reallocate memory
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: find_index_by_id
// PURPOSE : Searches the array for a matching student ID.
// RETURNS : index (0..arr_size-1) -> if found
//           -1 -> if not found
// -----------------------------------------------------------------------------
int find_index_by_id(int id) {
    for (size_t i = 0; i < arr_size; i++) {
        if (arr[i].id == id) //compare student ID
            return (int)i;   //return matching index
    }

    return -1; //if no match found
}


// -----------------------------------------------------------------------------
// FUNCTION: print_student_record
// PURPOSE : Displays a single student's details in a formatted table row.
//           Different colour output based on student's mark:
//           - RED(50) < YELLOW < GREEN(80)
//           - Excellent Grade: 80 and above
//           - Average Grade: between 50 and 80
//           - Failing Grade: below 50
// ACCEPTS : Student Object
// -----------------------------------------------------------------------------
void print_student_record(const Student *currentStudent) {
    const char *colour = RESET; //Default Colour: White

    if (currentStudent->mark >= 80) { //excellent
        colour = GREEN;
    }
    else if (currentStudent->mark < 50) { //failing
        colour = RED;
    }
    else { //average
        colour = YELLOW;
    }

    //Print data in aligned columns
    printf("%-10d %-20s %-30s %s%-6.1f%s\n",
           currentStudent->id,
           currentStudent->name,
           currentStudent->programme,
           colour, currentStudent->mark, RESET);
}

// -----------------------------------------------------------------------------
// FUNCTION: parse_line
// PURPOSE : Reads a line from the database file (.txt) and extracts:
//           - Student ID (tokens[0])
//           - First name + Last name (tokens[1] + tokens[2])
//           - Programme (tokens[3] + ... + tokens[n-2])
//           - Mark (tokens[n-1])
// RETURNS : 1 -> successful parse into studentObject
//           0 -> line does not contain enough data
// -----------------------------------------------------------------------------
int parse_line(const char *line, Student *studentObject) {
    char tempBuf[512]; //temporary buffer to store line for edit
    strncpy(tempBuf, line, sizeof(tempBuf) - 1);
    tempBuf[sizeof(tempBuf) - 1] = '\0'; //Null terminate Buffer
    tempBuf[strcspn(tempBuf, "\n")] = '\0'; //replace newline

    //Split buffer into tokens by space and tabs
    char *tokens[64]; //64 character pointers
    int tokenCount = 0;

    char *currentToken = strtok(tempBuf, " \t"); //First token
    while (currentToken != NULL && tokenCount < 64) { //store tokens until NULL or count reaches 64
        tokens[tokenCount] = currentToken;
        tokenCount++;
        currentToken = strtok(NULL, " \t"); //Continue searching for next token
    }

    if (tokenCount < 4) return 0; //must have 5 properties in Student Object, if not -> exit

    //Store ID into studentObject
    char *endPtr;
    long parsedId = strtol(tokens[0], &endPtr, 10);
    if (endPtr == tokens[0] || *endPtr != '\0' || parsedId < 0 || parsedId > INT_MAX) {
        return 0; //Invalid ID: checks nochange, non-numeric, negative, overflow
    }
    studentObject->id = (int)parsedId;

    //Store marks into studentObject
    char *endPtr2;
    float parsedMark = strtof(tokens[tokenCount - 1], &endPtr2);
    if (endPtr2 == tokens[tokenCount - 1] || *endPtr2 != '\0' || parsedMark < 0.0f || parsedMark > 100.0f) {
        return 0; //Invalid marks: checks nochange, non-numeric, out of range
    }
    studentObject->mark = parsedMark;

    //Store name into studentObject
    char nameBuffer[128] = "";
    snprintf(nameBuffer, sizeof(nameBuffer), "%s %s", tokens[1], tokens[2]); //combine first and last name
    strncpy(studentObject->name, nameBuffer, MAX_STR - 1);
    studentObject->name[MAX_STR - 1] = '\0'; //Null terminate name

    //Store programme into studentObject
    char programmeBuffer[256] = "";
    for (int i = 3; i < tokenCount - 1; i++) {
        strcat(programmeBuffer, tokens[i]); //append token to buffer
        if (i < tokenCount - 2) //append space, but not for last word
            strcat(programmeBuffer, " "); 
    }
    strncpy(studentObject->programme, programmeBuffer, MAX_STR - 1);
    studentObject->programme[MAX_STR - 1] = '\0';

    return 1;
}


/* --------------------------------------------------- */
/*  Helper functions for update & delete operations    */
/* --------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: trim_newline
// PURPOSE : Removes trailing '\n' or '\r' for string pointer
// -----------------------------------------------------------------------------
static void trim_newline(char *string) {

    if (string == NULL) return; 

    size_t stringLength = strlen(string);

    while (stringLength != 0 && (string[stringLength-1] == '\n' || string[stringLength-1] == '\r'))
    {
        stringLength--;
        string[stringLength] = '\0';
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: print_diff_row
// PURPOSE : Shows a side-by-side comparison of BEFORE vs AFTER values 
//           for a single field, highlighting changed fields in GREEN.
// -----------------------------------------------------------------------------
static void print_diff_row(const char *label, const char *before, const char *after)
{
    int isChanged = strcmp(before, after) != 0;

    //Print:
    //Field | BeforeValue | AfterValue (coloured if changed)
    printf("%-12s | %-30s | %s%-30s%s\n",
           label,
           before,
           isChanged ? GREEN : RESET,   //Colour AFTER value if changed
           after,
           isChanged ? RESET : RESET);
}

// -----------------------------------------------------------------------------
// FUNCTION: prompt_edit_str
// PURPOSE : Asks the user whether they want to edit a field's string value.
// USE     : Press ENTER -> keep old value
// RETURNS : 1 -> value changed
//           0 -> kept old value
// -----------------------------------------------------------------------------
static int prompt_edit_str(const char *fieldLabel, char *stringDestination, size_t cap, const char *current)
{
    char userBuffer[256];

    printf("%s (Enter to keep \"%s\"): ", fieldLabel, current); //ask user

    //User Input
    if (!fgets(userBuffer, sizeof(userBuffer), stdin)) { //If read fails(etc interrupts) -> no change
        return 0;
    }

    if (userBuffer[0] == '\n') //User presses enter -> no change
        return 0;

    trim_newline(userBuffer);
    strncpy(stringDestination, userBuffer, cap - 1);
    stringDestination[cap - 1] = '\0';

    return 1;
}


// -----------------------------------------------------------------------------
// FUNCTION: prompt_edit_mark
// PURPOSE : Allows the user to update a student's mark, with validation.
// USE     : Press ENTER -> keep old mark
//           Otherwise, must enter a valid number between 0 and 100.
// RETURNS : 1 -> marks changed
//           0 -> kept old marks
// -----------------------------------------------------------------------------
static int prompt_edit_mark(float *outputMark, float currentMark)
{
    char userBuffer[128];

    printf("Mark (Enter to keep %.1f): ", currentMark);

    if (!fgets(userBuffer, sizeof(userBuffer), stdin) || userBuffer[0] == '\n') //User presses enter -> no change
    {
        return 0;
    }
    trim_newline(userBuffer);

    //Convert string input to number
    char *endPtr = NULL;
    double markValue = strtod(userBuffer, &endPtr);

    //Validate input:
    //Check endPtr != '\0' means non-numeric, endPtr == userBuffer means no conversion
    //Ensures marks are between 0 and 100
    if (endPtr == userBuffer || *endPtr != '\0' || markValue < 0.0 || markValue > 100.0) {
        printf(RED "Invalid mark. Please enter a number from 0 to 100.\n" RESET);
        return prompt_edit_mark(outputMark, currentMark);
    }
    *outputMark = (float)markValue;

    return 1;
}


// -----------------------------------------------------------------------------
// FUNCTION: confirm_delete_by_id
// PURPOSE : Protects against accidental deletion by requiring user to type the exact ID to confirm
//           Typing 'N' cancels the operation
// RETURNS : 1 -> confirmed (user entered matching ID)
//           0 -> cancelled / mismatch
// -----------------------------------------------------------------------------
static int confirm_delete_by_id(int expectedId)
{
    printf("Type the ID " BOLD "%d" RESET " to confirm delete (or 'N' to cancel): ", expectedId);

    char userBuffer[64];
    if (!fgets(userBuffer, sizeof(userBuffer), stdin)) //check if userbuffer is null
    {
        return 0;
    }
    trim_newline(userBuffer);

    //Condition for cancelling operation
    if (userBuffer[0] && (userBuffer[0] == 'n' || userBuffer[0] == 'N') && userBuffer[1] == '\0')
    {
        return 0;
    }

    //Convert string input to number
    char *endPtr = NULL;
    long inputID = strtol(userBuffer, &endPtr, 10);

    //Check endPtr != '\0' means non-numeric, endPtr == userBuffer means no conversion
    if (endPtr == userBuffer || *endPtr != '\0')
    {
        return 0;
    }

    //Returns TRUE if the number matches expected ID
    return (inputID == expectedId);
}



/* ---------------------------------------------------- */
/* Core Operations                                      */
/* ---------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: open_db
// PURPOSE : Opens a .txt database file, reads student records, parses them, and loads them into Student array.
//
// DETAILS :
//   - Validates that the file exists and ends with ".txt"
//   - Skips the metadata/header (first 5 lines)
//   - Calls parse_line() to extract data for each line
//   - Automatically expands the array using ensure_cap()
//   - Logs the action in the audit log
//
// RETURNS : 1 -> success
//           0 -> failure (file missing or wrong format)
// -----------------------------------------------------------------------------
int open_db(const char *filePath) {
    FILE *filePtr;
    filePtr = fopen(filePath, "r"); //open file in read mode

    if (filePtr == NULL) { //file not found or cannot be opened.
        printf("CMS: Failed to open \"%s\" file not found!\n", filePath);
        return 0;
    }

    int filePathLength = strlen(filePath);
    if (filePathLength <= 4) { //Check for non *.txt
        printf("CMS: File is not a txt file.\n");
        return 0;
    }
    if (!(filePath[filePathLength - 1] == 't' && //check extension
          filePath[filePathLength - 2] == 'x' &&
          filePath[filePathLength - 3] == 't' &&
          filePath[filePathLength - 4] == '.')) 
    {
        printf("CMS: File is not a txt file.\n");
        return 0;
    }

    arr_size = 0; //Reset Student array length
    char currentFileLine[512];
    int lineNumber = 0; //Line number tracker, to skip headers

    while (fgets(currentFileLine, sizeof(currentFileLine), filePtr)) {
        lineNumber++; //Line number will increment based on number of iteration
        if (lineNumber <= 5) { //Skip metadata and table header
            continue;
        }

        //Parse student
        Student currentStudent;
        if (parse_line(currentFileLine, &currentStudent)) {
            ensure_cap();

            //Store into student array
            arr[arr_size] = currentStudent;
            arr_size++;
        } else { //parse_line returned 0
            //Invalid line format, skip
            printf(YELLOW "CMS Warning: Skipping invalid line %d in file.\n" RESET, lineNumber);
        }
    }

    fclose(filePtr);

    printf("CMS: \"%s\" opened (%zu records)\n", filePath, arr_size);

    audit_log("OPEN %s (%zu records)", filePath, arr_size); //Audit log

    last_op.op = OP_NONE; //Reset Undo history

    return 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: show_all
// PURPOSE : Prints a nicely formatted table of all student records currently stored in memory.
// -----------------------------------------------------------------------------
void show_all(void) {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    //Print table header with formatting and colour
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    //Loop through each record and print
    for (size_t i = 0; i < arr_size; i++) {
        //Decide colour based on marks
        const char *colour = RESET;
        if (arr[i].mark >= 80)
            colour = GREEN;
        else if (arr[i].mark < 50)
            colour = RED;
        else
            colour = YELLOW;

        //Print row in formatted columns
        printf("%-10d %-20s %-30s %s%-6.1f%s\n",
               arr[i].id,
               arr[i].name,
               arr[i].programme,
               colour, arr[i].mark, RESET);
    }
}

// -----------------------------------------------------------------------------
// COMPARATOR: idAsc
// PURPOSE   : Used by qsort() to sort students by ID in ascending order.
// -----------------------------------------------------------------------------
int idAsc(const void* a, const void* b) {
    Student* student_a = (Student*)a;
    Student* student_b = (Student*)b;
    return student_a->id - student_b->id;
}

// -----------------------------------------------------------------------------
// COMPARATOR: idDesc
// PURPOSE   : Sort students by ID in descending order.
// -----------------------------------------------------------------------------
int idDesc(const void* a, const void* b) {
    Student* student_a = (Student*)a;
    Student* student_b = (Student*)b;
    return student_b->id - student_a->id;
}

// -----------------------------------------------------------------------------
// COMPARATOR: markAsc
// PURPOSE   : Sort by mark from lowest -> highest.
// -----------------------------------------------------------------------------
int markAsc(const void* a, const void* b) {
    float mark_a = ((Student*)a)->mark;
    float mark_b = ((Student*)b)->mark;

    if (mark_a > mark_b) return 1;
    if (mark_a < mark_b) return -1;
    return 0;   // equal
}

// -----------------------------------------------------------------------------
// COMPARATOR: markDesc
// PURPOSE   : Sort by mark from highest -> lowest.
// -----------------------------------------------------------------------------
int markDesc(const void* a, const void* b) {
    float mark_a = ((Student*)a)->mark;
    float mark_b = ((Student*)b)->mark;

    if (mark_a > mark_b) return -1;
    if (mark_a < mark_b) return 1;
    return 0;
}

// -----------------------------------------------------------------------------
// FUNCTION: showSorted
// PURPOSE : Sorts the array based on user command then reprints all records.
// ACCEPTS : field = "ID" or "MARK"
//           order = "ASC" or "DESC"
// -----------------------------------------------------------------------------
void showSorted(const char* field, const char* order) {
    //Determine field to sort by
    if (strcmp(field, "ID") == 0) {
        //Determine sort direction
        if (strcmp(order, "ASC") == 0)
            qsort(arr, arr_size, sizeof(Student), idAsc);

        else if (strcmp(order, "DESC") == 0)
            qsort(arr, arr_size, sizeof(Student), idDesc);
    }
    else if (strcmp(field, "MARK") == 0) {
        if (strcmp(order, "ASC") == 0)
            qsort(arr, arr_size, sizeof(Student), markAsc);

        else if (strcmp(order, "DESC") == 0)
            qsort(arr, arr_size, sizeof(Student), markDesc);
    }

    //After sorting, print the updated table
    show_all();
}

// -----------------------------------------------------------------------------
// FUNCTION: insert_record
// PURPOSE : Inserts a new student object into the dynamic array.
// BEHAVIOUR:
//   - Prevents duplicate IDs
//   - Expands array if needed
//   - Logs the insertion
//   - Stores undo information
// -----------------------------------------------------------------------------
void insert_record(Student studentObject) {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    //Reject duplicate IDs
    if (find_index_by_id(studentObject.id) != -1) {
        printf("CMS: ID already exists!\n");
        return;
    }

    //Ensure array has enough space before insert
    ensure_cap();

    //Insert new student to the array
    arr[arr_size] = studentObject;
    arr_size++;

    printf("CMS: Record inserted successfully!\n");

    audit_log("INSERT %d %s %s %.1f", studentObject.id, studentObject.name, studentObject.programme, studentObject.mark); //Audit Log

    //Prepare for undo
    last_op.op = OP_INSERT;
    last_op.after = studentObject;
}

// -----------------------------------------------------------------------------
// FUNCTION: query
// PURPOSE : Looks up a student by ID and prints their record.
// -----------------------------------------------------------------------------
void query(int studentId) {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int studentIndex = find_index_by_id(studentId);

    if (studentIndex < 0) { //If student ID not found (-1), exit
        printf("CMS: The record with ID %d does not exist.\n", studentId);
        return;
    }

    //Print Header
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET, "ID", "Name", "Programme", "Mark");

    //Decide colour based on marks
    const char *colour = RESET;
    if (arr[studentIndex].mark >= 80)
        colour = GREEN;
    else if (arr[studentIndex].mark < 50)
        colour = RED;
    else
        colour = YELLOW;

    //Print the student record
    printf("%-10d %-20s %-30s %s%-6.1f%s\n",
           arr[studentIndex].id,
           arr[studentIndex].name,
           arr[studentIndex].programme,
           colour, arr[studentIndex].mark, RESET);
}

// -----------------------------------------------------------------------------
// FUNCTION: update
// PURPOSE : Modifies an existing student record.
// DETAILS :
//   1. Look up student by ID
//   2. Show current data
//   3. Ask user which fields to edit
//   4. Show a BEFORE/AFTER comparison (diff)
//   5. Ask for confirmation
//   6. Save changes and update undo log
// -----------------------------------------------------------------------------
void update(int studentID) {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int studentIndex = find_index_by_id(studentID);
    if (studentIndex < 0) {
        printf("CMS: The record with ID %d does not exist.\n", studentID);
        return;
    }

    //Save copies of BEFORE and AFTER states for diff & undo
    Student before = arr[studentIndex];
    Student after  = before;

    //Display current record
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET, "ID", "Name", "Programme", "Mark");

    print_student_record(&before);

    //Allow user to edit fields (ENTER == keep old value)
    int changed = 0;

    changed |= prompt_edit_str("Name",
                               after.name, MAX_STR,
                               before.name);

    changed |= prompt_edit_str("Programme",
                               after.programme, MAX_STR,
                               before.programme);

    changed |= prompt_edit_mark(&after.mark,
                                before.mark);

    //If no field changed, abort update
    if (!changed) {
        printf(YELLOW "No changes detected. Update cancelled.\n" RESET);
        return;
    }

    // --------------------------------------------
    // Show before/after diff table for confirmation
    // --------------------------------------------
    printf("\n" BOLD "Review changes:" RESET "\n");
    printf("%-12s | %-30s | %-30s\n",
           "Field", "Before", "After");
    printf("-------------+--------------------------------+--------------------------------\n");

    print_diff_row("Name",      before.name,      after.name);
    print_diff_row("Programme", before.programme, after.programme);

    char bmark[32], amark[32];
    snprintf(bmark, sizeof(bmark), "%.1f", before.mark);
    snprintf(amark, sizeof(amark), "%.1f", after.mark);
    print_diff_row("Mark", bmark, amark);

    //Confirm if user wants to apply changes
    char confirm[32];
    printf("\nConfirm update (Y/N)? ");
    if (!fgets(confirm, sizeof(confirm), stdin)) { //Check if NULL
        printf("Cancelled.\n");
        return;
    }
    //Convert lowercase and check 'y'
    for (char *confirmPtr = confirm; *confirmPtr; ++confirmPtr)
    {
        *confirmPtr = (char)tolower((unsigned char)*confirmPtr);
    }
    if (confirm[0] != 'y') {
        printf("Cancelled.\n");
        return;
    }

    //Apply changes
    arr[studentIndex] = after;

    printf(GREEN "CMS: Record updated.\n" RESET);

    //Log update details
    audit_log("UPDATE %d | \"%s\" -> \"%s\" | \"%s\" -> \"%s\" | %.1f -> %.1f", 
        studentID,
        before.name, after.name,
        before.programme, after.programme,
        before.mark, after.mark);

    //Prepare for undo
    last_op.op = OP_UPDATE;
    last_op.before = before;
    last_op.after  = after;
}

// -----------------------------------------------------------------------------
// FUNCTION: delete
// PURPOSE : Removes a student record from the system.
// DETAILS :
//   1. Look up student by ID
//   2. Display record about to be deleted
//   3. Require user to type ID to confirm
//   4. Remove from array (swap-delete method)
//   5. Write to audit log
//   6. Save undo info
// -----------------------------------------------------------------------------
void delete(int studentID) {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int studentIndex = find_index_by_id(studentID);
    if (studentIndex < 0) {
        printf("CMS: The record with ID %d does not exist.\n", studentID);
        return;
    }

    //Backup the record so UNDO can restore it
    Student before = arr[studentIndex];

    //Show record before deletion
    printf("\n" BOLD "About to delete this record:" RESET "\n");
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET, "ID", "Name", "Programme", "Mark");

    print_student_record(&before);

    //Require exact ID confirmation
    if (!confirm_delete_by_id(before.id)) {
        printf(YELLOW "Cancelled.\n" RESET);
        return;
    }

    //Delete by overwriting this index with last record (O(1))
    arr[studentIndex] = arr[arr_size - 1];
    arr_size--;

    printf(GREEN "CMS: Record deleted.\n" RESET);

    audit_log("DELETE %d | \"%s\" | \"%s\" | %.1f",
        before.id, before.name,
        before.programme, before.mark);

    //Prepare undo record
    last_op.op = OP_DELETE;
    last_op.before = before;
}

// -----------------------------------------------------------------------------
// FUNCTION: save
// PURPOSE : Writes the current student array into a .txt file in formatted table form, with metadata.
// DETAILS :
//   - Opens output file in write mode
//   - Writes database name + author list + table name
//   - Writes table header
//   - Writes every student record in formatted columns
//   - Write to audit log
// -----------------------------------------------------------------------------
void save() {
    if (arr_size == 0) { //No records in memory
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    FILE *filePtr = fopen(FILENAME, "w"); //will overwrite existing *.txt file
    if (filePtr == NULL) {
        printf("Save failed.\n");
        return;
    }

    //Write Metadata Headers
    fprintf(filePtr, "Database Name: P9_3-CMS\n");
    fprintf(filePtr, "Authors: Ryan, Glenn, Min Han, Jordan, Ben\n");
    fprintf(filePtr, "Table Name: StudentRecords\n\n");

    //Write Column Header
    fprintf(filePtr, "%-10s %-15s %-25s %-6s\n", "ID", "Name", "Programme", "Mark");

    //Iterate student array and write to each row
    for (size_t i = 0; i < arr_size; i++) {
        fprintf(filePtr, "%-10d %-15s %-25s %-6.1f\n",
            arr[i].id,
            arr[i].name,
            arr[i].programme,
            arr[i].mark);
    }

    fclose(filePtr);

    printf("CMS: Saved to \"%s\".\n", FILENAME);

    audit_log("SAVE %s", FILENAME); //Audit Logging Purposes
}

// -----------------------------------------------------------------------------
// FUNCTION: summary
// PURPOSE : Displays class-wide statistics including:
//           - total student count
//           - average mark
//           - highest mark + student name
//           - lowest mark + student name
// DETAILS :
//   - Iterate through all records once
//   - Track running total, max, min, and indices
// -----------------------------------------------------------------------------
void summary() {
    if (arr_size == 0) { //No records in memory -> cannot summarise
        printf("No students available.\n");
        return;
    }

    int total = arr_size; //Total number of students
    double sum = 0.0; //Running total of all marks

    //Initialise highest/lowest values using first student's mark
    double highest = arr[0].mark;
    double lowest  = arr[0].mark;
    size_t hi_index = 0; //Index of student with highest mark
    size_t lo_index = 0; //Index of student with lowest mark

    // -----------------------------------------------------
    // Scan through all records to compute statistics
    // -----------------------------------------------------
    for (size_t i = 0; i < arr_size; i++) {

        double m = arr[i].mark;
        sum += m;     //Add mark to running sum

        //Track highest mark
        if (m > highest) {
            highest = m;
            hi_index = i;
        }

        //Track lowest mark
        if (m < lowest) {
            lowest = m;
            lo_index = i;
        }
    }

    double average = sum / total;   //Class average

    // -----------------------------------------------------
    // Print results in colored, formatted output
    // -----------------------------------------------------
    printf(CYAN "===== Student Summary =====\n" RESET);

    printf("Total students :  %d\n", total);

    printf("Average mark   :");
    printf(YELLOW " % .2f\n" RESET, average);

    printf("Highest mark   : ");
    printf(GREEN "% .1f (% s)\n" RESET, highest, arr[hi_index].name);

    printf("Lowest mark    :");
    printf(RED   " % .1f (% s)\n" RESET, lowest, arr[lo_index].name);

    printf(CYAN "===========================\n" RESET);
}

// -----------------------------------------------------------------------------
// FUNCTION: undo
// PURPOSE : Reverts the last modifying operation.
// SUPPORTS:
//   - Undo INSERT: remove the last inserted student
//   - Undo DELETE: reinsert the deleted student
//   - Undo UPDATE: restore record to previous state
//
// NOTES:
//   - Only ONE level of undo is supported
//   - last_op struct stores the type of operation,
//     and before/after student snapshots
// -----------------------------------------------------------------------------
void undo() {
    //If no operation to undo
    if (last_op.op == OP_NONE) {
        printf(YELLOW "CMS: Nothing to undo.\n" RESET);
        return;
    }

    //Visual header
    printf("\n" BOLD "===== Performing UNDO operation =====" RESET "\n");

    //Print table headers
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET, "ID", "Name", "Programme", "Mark");

    printf(CYAN "------------------------------------------------------------------\n" RESET);

    // -------------------------------------------------------------------------
    // CASE 1: Undo INSERT -> Remove the record that was inserted
    // -------------------------------------------------------------------------
    if (last_op.op == OP_INSERT) {

        //Locate the inserted record using its ID
        int i = find_index_by_id(last_op.after.id);

        //Only undo if the record is still present
        if (i >= 0 && arr_size > 0) {

            printf(YELLOW "Removed record:\n" RESET);
            print_student_record(&arr[i]);

            //Delete using swap-delete for O(1) removal
            arr[i] = arr[arr_size - 1];
            arr_size--;

            printf(GREEN "CMS: Undo INSERT successful (Record ID %d removed).\n" RESET, last_op.after.id);

            audit_log("UNDO INSERT (ID %d removed)", last_op.after.id);
        }
        else {
            printf(RED "CMS Error: Undo failed. Record ID %d not found.\n" RESET, last_op.after.id);

            audit_log("UNDO INSERT failed (ID %d not found)", last_op.after.id);
        }
    }

    // -------------------------------------------------------------------------
    // CASE 2: Undo DELETE -> Re-insert the previously deleted record
    // -------------------------------------------------------------------------
    else if (last_op.op == OP_DELETE) {
        //Ensure enough memory in array before inserting deleted student
        ensure_cap();

        //Reinsert deleted student
        arr[arr_size] = last_op.before;
        arr_size++;

        printf(GREEN "Re-inserted record:\n" RESET);
        print_student_record(&last_op.before);

        printf(GREEN "CMS: Undo DELETE successful (Record ID %d re-inserted).\n" RESET, last_op.before.id);

        audit_log("UNDO DELETE (ID %d re-inserted)", last_op.before.id);
    }

    // -------------------------------------------------------------------------
    // CASE 3: Undo UPDATE -> Restore the BEFORE state
    // -------------------------------------------------------------------------
    else if (last_op.op == OP_UPDATE) {

        //Find the record using ID from the "after" snapshot
        //because the user might have edited the name/programme
        int i = find_index_by_id(last_op.after.id);

        if (i >= 0) {

            printf(YELLOW "Restoring record from state before update:\n" RESET);
            print_student_record(&last_op.before);

            //Restore original version
            arr[i] = last_op.before;

            printf(GREEN "CMS: Undo UPDATE successful (Record ID %d reverted).\n" RESET, last_op.before.id);

            audit_log("UNDO UPDATE (ID %d restored)", last_op.before.id);
        }
        else {

            printf(RED "CMS Error: Undo failed. Record ID %d not found.\n" RESET, last_op.after.id);

            audit_log("UNDO UPDATE failed (ID %d not found)", last_op.after.id);
        }
    }

    //Clear undo history so cannot undo twice
    printf(BOLD "====================================" RESET "\n");

    last_op.op = OP_NONE;
}


/* ---------------------------------------------------- */
/* Command Loop                                         */
/* ---------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: main
// PURPOSE : Interactive CMS main loop. Waits for user input, identifies the command, and calls the appropriate function.
// DETAILS :
//   - Reads and parses commands like OPEN, SHOW, INSERT, DELETE, UPDATE, etc.
//   - Supports multi-word commands using sscanf()
//   - Uses strcasecmp() for case-insensitive matching
//   - Runs until user types EXIT
// -----------------------------------------------------------------------------
int main(void) {
    // -------------------------------------------------------------------------
    // Variable Buffers for reading user commands and breaking them into components
    // -------------------------------------------------------------------------
    char userBuffer[256]; //Full line typed by user
    char command[64];     //First word of command
    char arg1[64];        //Optional argument 1
    char arg2[64];        //Optional argument 2
    char arg3[64];        //Optional argument 3

    //For printing current time
    time_t now = time(NULL); //Get current time
    struct tm *t = localtime(&now); //Convert human readable format
    char datetime[64];
    strftime(datetime, sizeof(datetime),
            "%A, %d %B %Y, %I:%M %p", t);

    printf(RED"============================================== DECLARATION ==============================================\n" RESET);
    printf("SIT's policy on copying does not allow the students to copy source code as well as assessment solutions from another person AI or other places. "
        "It is the students' responsibility to guarantee that their assessment solutions are their own work. "
        "Meanwhile, the students must also ensure that their work is not accessible by others. "
        "Where such plagiarism is detected, both of the assessments involved will receive ZERO mark.\n\n");
    printf("We hereby declare that:\n");
    printf("    - We fully understand and agree to the abovementioned plagiarism policy.\n");
    printf("    - We did not copy any code from others or from other places.\n");
    printf("    - We did not share our codes with others or upload to any other places for public access and will not do that in the future.\n");
    printf("    - We agree that our project will receive Zero mark if there is any plagiarism detected.\n");
    printf("    - We agree that we will not disclose any information or material of the group project to others or upload to any other places for public access.\n");
    printf("    - We agree that we did not copy any code directly from AI generated sources.\n\n");

    printf("Declared by: P9-3\n"
        "Team members:\n");
    printf("    1. Ng Si Yuan Ryan\n");
    printf("    2. Ong Tiong Yew Glenn\n");
    printf("    3. Lim Ler Yang, Jordan\n");
    printf("    4. Chong Min Han\n");
    printf("    5. Wong Kok Sheng Benjamin\n\n");

    printf("Date: 24th November 2025\n");
    printf(RED"=========================================================================================================\n\n"RESET);

    printf("Hello there! P9_3 Classroom Management System [CMS] Ready. Today is %s.\n", datetime);
    printf("Type HELP to display available commands.\n");

    // -------------------------------------------------------------------------
    // MAIN COMMAND LOOP
    // -------------------------------------------------------------------------
    while (1) {
        //Will always display the prompt "P9_3>"
        printf("P9_3> ");

        if (!fgets(userBuffer, sizeof(userBuffer), stdin)) //User Input
        {
            break; //If input fails (EOF), exit loop
        }
        userBuffer[strcspn(userBuffer, "\n")] = '\0'; //Remove trailing newline

        //Reset command buffers before parsing
        command[0] = arg1[0] = arg2[0] = arg3[0] = '\0';

        // ---------------------------------------------------------------------
        // Parse up to 4 tokens (command + 3 arguments)
        // Example: SHOW ALL SORT BY MARK DESC
        // Will map to:
        //    command = "SHOW"
        //    arg1    = "ALL"
        //    arg2    = "SORT"
        //    arg3    = "BY"
        // ---------------------------------------------------------------------
        int commandArgCount = sscanf(userBuffer, "%63s %63s %63s %63s", command, arg1, arg2, arg3);

        //Pressed ENTER -> restart loop
        if (commandArgCount < 1) continue;

        // ---------------------------------------------------------------------
        // COMMAND DISPATCHER
        // ---------------------------------------------------------------------

        //============================= OPEN =============================
        if (strcasecmp(command, "OPEN") == 0) {
            if (commandArgCount >= 2) { //OPEN <filename>
                open_db(arg1);
            }
            else {
                printf("Usage: OPEN filename\n");
            }
        }

        //============================= SHOW =============================
        else if (strcasecmp(command, "SHOW") == 0) {
            //Case 1: SHOW ALL
            if (strcasecmp(arg1, "ALL") == 0) {

                //Case 1a: SHOW ALL SORT BY <field> <order>
                if (strcasecmp(arg2, "SORT") == 0 && strcasecmp(arg3, "BY") == 0) {
                    
                    char field[16] = "\0";
                    char order[16] = "\0";

                    //Extract field + order from the rest of the text input
                    int numOfArgs = sscanf(userBuffer, "%*s %*s %*s %*s %15s %15s", field, order);

                    if (numOfArgs >= 1) {
                        //Convert field and order into uppercase
                        for (int i = 0; field[i]; i++) {
                            field[i] = toupper(field[i]);
                        }

                        for (int j = 0; order[j]; j++) {
                            order[j] = toupper(order[j]);
                        }

                        //If order provided -> use it
                        if (order[0]) {
                            showSorted(field, order);
                        }
                        else {
                            showSorted(field, "ASC"); //default to ASC
                        }
                    }
                }
                //Case 1b: SHOW ALL
                else {
                    show_all();
                }
            }

            //Case 2: SHOW SUMMARY
            else if (strcasecmp(arg1, "SUMMARY") == 0) {
                summary();
            }

            else {
                printf("Usage: SHOW ALL | SHOW SUMMARY | SHOW ALL SORT BY ...\n");
            }
        }

        //============================= INSERT =============================
        else if (strcasecmp(command, "INSERT") == 0) {

            Student newStudentObject = {0};
            char userBuffer[256] = {0};

            //ID
            printf("ID: ");
            if (!fgets(userBuffer, sizeof(userBuffer), stdin) || userBuffer[0] == '\n') {
                printf("CMS Error: ID cannot be empty.\n");
                continue;
            }
            userBuffer[strcspn(userBuffer, "\n")] = '\0'; //Trim newline
            //Cast ID(string) to int
            char *endPtr;
            long studentID = strtol(userBuffer, &endPtr, 10);
            if (*endPtr != '\0' || studentID <= 0) {
                printf("CMS Error: ID must be a positive number.\n");
                continue;
            }
            newStudentObject.id = (int)studentID;

            // Check if ID already exists
            if (query_exists(newStudentObject.id)) {
                printf("CMS Error: Student with ID %d already exists.\n", newStudentObject.id);
                continue;
            }

            //Name
            printf("Name: ");
            if (!fgets(userBuffer, sizeof(userBuffer), stdin) || userBuffer[0] == '\n') {
                printf("CMS Error: Name cannot be empty.\n");
                continue;
            }
            userBuffer[strcspn(userBuffer, "\n")] = '\0'; //Trim newline
            if (strlen(userBuffer) >= MAX_STR) { //Check: Name exceed MAX_STR
                printf("CMS Error: Name too long. Maximum %d characters.\n", MAX_STR - 1);
                continue;
            }
            strncpy(newStudentObject.name, userBuffer, MAX_STR - 1); //Copy to student object
            newStudentObject.name[MAX_STR - 1] = '\0';

            //Programme
            printf("Programme: ");
            if (!fgets(userBuffer, sizeof(userBuffer), stdin) || userBuffer[0] == '\n') {
                printf("CMS Error: Programme cannot be empty.\n");
                continue;
            }
            userBuffer[strcspn(userBuffer, "\n")] = '\0'; //Trim newline
            if (strlen(userBuffer) >= MAX_STR) { //Check: Programme exceed MAX_STR
                printf("CMS Error: Programme name too long. Maximum %d characters.\n", MAX_STR - 1);
                continue;
            }
            strncpy(newStudentObject.programme, userBuffer, MAX_STR - 1); //Copy to student object
            newStudentObject.programme[MAX_STR - 1] = '\0';

            //Mark
            printf("Mark: ");
            if (!fgets(userBuffer, sizeof(userBuffer), stdin)) {
                printf("CMS Error: Invalid mark input.\n");
                continue;
            }
            userBuffer[strcspn(userBuffer, "\n")] = '\0'; //Trim newline
            //Cast Marks(string) to float
            char *endPtr2;
            float studentMarks = strtof(userBuffer, &endPtr2);
            if (*endPtr2 != '\0' || studentMarks < 0.0f || studentMarks > 100.0f) { //Also checks for marks between 0 and 100
                printf("CMS Error: Mark must be a valid number between 0 and 100.\n");
                continue;
            }
            newStudentObject.mark = studentMarks;

            //Insert record into current student array
            insert_record(newStudentObject);
        }

        //============================= QUERY =============================
        else if (strcasecmp(command, "QUERY") == 0) {

            if (commandArgCount >= 2) {
                int id = atoi(arg1);
                query(id);
            }
            else {
                printf("Usage: QUERY <ID>\n");
            }
        }

        //============================= UPDATE =============================
        else if (strcasecmp(command, "UPDATE") == 0) {

            if (commandArgCount >= 2) {
                int id = atoi(arg1);
                update(id);
            }
            else {
                printf("Usage: UPDATE <ID>\n");
            }
        }

        //============================= DELETE =============================
        else if (strcasecmp(command, "DELETE") == 0) {

            if (commandArgCount >= 2) {
                int id = atoi(arg1);
                delete(id);
            }
            else {
                printf("Usage: DELETE <ID>\n");
            }
        }

        //============================= SAVE =============================
        else if (strcasecmp(command, "SAVE") == 0) {

            save();
        }

        //============================= UNDO =============================
        else if (strcasecmp(command, "UNDO") == 0) {

            undo();
        }

        //============================= HELP =============================
        else if (strcasecmp(command, "HELP") == 0) {

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
        }

        //============================= EXIT =============================
        else if (strcasecmp(command, "EXIT") == 0) {

            break; //Leave main loop
        }

        //============================= UNKNOWN =============================
        else {

            printf("Unknown command. Type HELP to display available commands.\n");
        }
    }
    free(arr); //Free student array before exit

    return 0;
}



