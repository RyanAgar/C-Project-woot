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

    studentObject->id = atoi(tokens[0]); //convert integer id
    studentObject->mark = atof(tokens[tokenCount - 1]); //convert float marks

    char nameBuffer[128] = "";
    snprintf(nameBuffer, sizeof(nameBuffer), "%s %s", tokens[1], tokens[2]); //combine first and last name
    strncpy(studentObject->name, nameBuffer, MAX_STR - 1);
    studentObject->name[MAX_STR - 1] = '\0'; //Null terminate name

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


/* ------------------------------ */
/*  Helpers for update/ delete    */
/* ------------------------------ */

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
static void print_diff_row(const char *label,
                           const char *before,
                           const char *after)
{
    // Determine if this field actually changed.
    int changed = strcmp(before, after) != 0;

    // Print:
    // Field | BeforeValue | AfterValue (colored if changed)
    printf("%-12s | %-30s | %s%-30s%s\n",
           label,
           before,
           changed ? GREEN : RESET,   // Color AFTER value if changed
           after,
           changed ? RESET : RESET);
}


// -----------------------------------------------------------------------------
// FUNCTION: prompt_edit_str
// PURPOSE : Asks the user whether they want to edit a field's string value.
// BEHAVIOR: Press ENTER → keep old value
// RETURNS : 1 → value changed
//           0 → kept old value
// -----------------------------------------------------------------------------
static int prompt_edit_str(const char *label,
                           char *dst,
                           size_t cap,
                           const char *current)
{
    char buf[256];

    // Ask user, showing current value inside quotes
    printf("%s (Enter to keep \"%s\"): ", label, current);

    // Try reading user input
    if (!fgets(buf, sizeof(buf), stdin)) {
        buf[0] = '\0';   // If read failed, treat as "no change"
    }

    // If user pressed ENTER immediately, keep original
    if (buf[0] == '\n')
        return 0;  // no change

    // Otherwise, trim newline and copy new value into dst
    trim_newline(buf);
    strncpy(dst, buf, cap - 1);
    dst[cap - 1] = '\0';

    return 1;  // changed
}


// -----------------------------------------------------------------------------
// FUNCTION: prompt_edit_mark
// PURPOSE : Allows user to update a student's mark, with validation.
// BEHAVIOR: ENTER → keep old mark
//           Otherwise must enter a valid number between 0 and 100.
// RETURNS : 1 → mark changed
//           0 → kept same mark
// -----------------------------------------------------------------------------
static int prompt_edit_mark(float *out, float current)
{
    char buf[128];

    printf("Mark (Enter to keep %.1f): ", current);

    // If user presses ENTER → keep original
    if (!fgets(buf, sizeof(buf), stdin) || buf[0] == '\n')
        return 0;

    trim_newline(buf);

    // Convert input to number
    char *endp = NULL;
    double mv = strtod(buf, &endp);

    // Validate input:
    // strtod fails if endp == buf OR endp has extra invalid chars
    // Also ensure number is between 0 and 100
    if (endp == buf || *endp != '\0' || mv < 0.0 || mv > 100.0) {

        // Error message with recursive retry
        printf(RED "Invalid mark. Please enter a number from 0 to 100.\n" RESET);
        return prompt_edit_mark(out, current);
    }

    // Accept valid integer/float mark
    *out = (float)mv;
    return 1;
}


// -----------------------------------------------------------------------------
// FUNCTION: confirm_delete_by_id
// PURPOSE : Protects against accidental deletion by requiring user to type
//           the exact ID to confirm. Typing 'N' cancels the operation.
// RETURNS : 1 → confirmed (user entered matching ID)
//           0 → cancelled / mismatch
// -----------------------------------------------------------------------------
static int confirm_delete_by_id(int expected_id)
{
    char buf[64];

    // Display prompt:
    // Example:
    // Type the ID 1003 to confirm delete (or 'N' to cancel):
    printf("Type the ID " BOLD "%d" RESET
           " to confirm delete (or 'N' to cancel): ",
           expected_id);

    // Read user input
    if (!fgets(buf, sizeof(buf), stdin))
        return 0;

    trim_newline(buf);

    // If user entered just 'N' or 'n', abort delete
    if (buf[0] && (buf[0] == 'n' || buf[0] == 'N') && buf[1] == '\0')
        return 0;

    // Try converting input to integer
    char *endp = NULL;
    long v = strtol(buf, &endp, 10);

    // Validation: must be a clean integer (no leftover chars)
    if (endp == buf || *endp != '\0')
        return 0;

    // Only return TRUE if the number matches expected ID
    return (v == expected_id);
}



/* ---------------------------------------------------- */
/* Core Operations                                      */
/* ---------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: open_db
// PURPOSE : Opens a .txt database file, reads student records, parses them,
//           and loads them into the dynamic array.
//
// DETAILS :
//   - Validates that the file exists.
//   - Checks that the file extension is ".txt".
//   - Skips the metadata/header (first 5 lines).
//   - For each remaining line, calls parse_line() to extract data.
//   - Automatically expands the array using ensure_cap().
//   - Logs the action in the audit log.
//
// RETURNS : 1 → success
//           0 → failure (file missing or wrong format)
// -----------------------------------------------------------------------------
int open_db(const char *filePath) {

    FILE *filePtr;

    // Attempt to open the file in read mode.
    filePtr = fopen(filePath, "r");

    // If fopen() returns NULL, file was not found or cannot be opened.
    if (filePtr == NULL) {
        printf("CMS: Failed to open \"%s\" file not found!\n", filePath);
        return 0;
    }

    // -------------------------------------------------------------------------
    // Validate file extension (must end with .txt)
    // -------------------------------------------------------------------------
    int filePathLength = strlen(filePath);

    // Minimum length = 5 (e.g., a.txt)
    if (filePathLength <= 4) {
        printf("CMS: File is not a txt file.\n");
        return 0;
    }

    // Check last 4 characters
    if (!(filePath[filePathLength - 1] == 't' &&
          filePath[filePathLength - 2] == 'x' &&
          filePath[filePathLength - 3] == 't' &&
          filePath[filePathLength - 4] == '.')) {

        printf("CMS: File is not a txt file.\n");
        return 0;
    }

    // -------------------------------------------------------------------------
    // Begin reading file contents
    // -------------------------------------------------------------------------
    arr_size = 0;                // Reset number of records before loading
    char currentFileLine[512];   // Buffer for reading each line
    int lineNumber = 0;          // Track line index (to skip headers)

    while (fgets(currentFileLine, sizeof(currentFileLine), filePtr)) {

        lineNumber++;  // Count every line read

        // Skip first 5 lines (metadata + table header)
        if (lineNumber <= 5) {
            continue;
        }

        // Parse the student record
        Student s;
        if (parse_line(currentFileLine, &s)) {

            // Ensure enough capacity in the array
            ensure_cap();

            // Store the parsed student struct
            arr[arr_size++] = s;
        }
    }

    // Close file after reading all lines
    fclose(filePtr);

    // Display message on screen
    printf("CMS: \"%s\" opened (%zu records)\n", filePath, arr_size);

    // Write to audit log
    audit_log("OPEN %s (%zu records)", filePath, arr_size);

    // Reset UNDO history since dataset changed
    last_op.op = OP_NONE;

    return 1;
}


// -----------------------------------------------------------------------------
// FUNCTION: show_all
// PURPOSE : Prints a nicely formatted table of all student records currently
//           stored in memory.
// -----------------------------------------------------------------------------
void show_all(void) {

    // Guard: No data loaded
    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    // Print table header with formatting and color
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    // Loop through each record and print it
    for (size_t i = 0; i < arr_size; i++) {

        // Decide color based on mark
        const char *color = RESET;
        if (arr[i].mark >= 80)
            color = GREEN;
        else if (arr[i].mark < 50)
            color = RED;
        else
            color = YELLOW;

        // Print row in formatted columns
        printf("%-10d %-20s %-30s %s%-6.1f%s\n",
               arr[i].id,
               arr[i].name,
               arr[i].programme,
               color, arr[i].mark, RESET);
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
// PURPOSE   : Sort by mark from lowest → highest.
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
// PURPOSE   : Sort by mark from highest → lowest.
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
// INPUTS  : field = "ID" or "MARK"
//           order = "ASC" or "DESC"
// -----------------------------------------------------------------------------
void showSorted(const char* field, const char* order) {

    // Determine field to sort by
    if (strcmp(field, "ID") == 0) {

        // Determine sort direction
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

    // After sorting, print the updated table
    show_all();
}



// -----------------------------------------------------------------------------
// FUNCTION: insert_record
// PURPOSE : Inserts a new student struct into the dynamic array.
// BEHAVIOR:
//   - Prevents duplicate IDs
//   - Expands array if needed
//   - Logs the insertion
//   - Stores undo information
// -----------------------------------------------------------------------------
void insert_record(Student s) {

    // Cannot insert until OPEN loads data
    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    // Reject duplicate IDs
    if (find_index_by_id(s.id) != -1) {
        printf("CMS: ID already exists!\n");
        return;
    }

    // Make sure array has enough space
    ensure_cap();

    // Append new student to the array
    arr[arr_size++] = s;

    printf("CMS: Record inserted successfully!\n");

    // Log the action
    audit_log("INSERT %d %s %s %.1f",
              s.id, s.name, s.programme, s.mark);

    // Prepare for undo
    last_op.op = OP_INSERT;
    last_op.after = s;
}


// -----------------------------------------------------------------------------
// FUNCTION: query
// PURPOSE : Looks up a student by ID and prints their record.
// -----------------------------------------------------------------------------
void query(int id) {

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int i = find_index_by_id(id);

    // If student ID not found
    if (i < 0) {
        printf("CMS: The record with ID %d does not exist.\n", id);
        return;
    }

    // Print header
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    // Reuse same color logic as show_all()
    const char *color = RESET;
    if (arr[i].mark >= 80)
        color = GREEN;
    else if (arr[i].mark < 50)
        color = RED;
    else
        color = YELLOW;

    // Print the student record
    printf("%-10d %-20s %-30s %s%-6.1f%s\n",
           arr[i].id,
           arr[i].name,
           arr[i].programme,
           color, arr[i].mark, RESET);
}


// -----------------------------------------------------------------------------
// FUNCTION: update
// PURPOSE : Modifies an existing student record.
// PROCESS :
//   1. Look up student by ID
//   2. Show current data
//   3. Ask user which fields to edit
//   4. Show a BEFORE/AFTER comparison (diff)
//   5. Ask for confirmation
//   6. Save changes and update undo log
// -----------------------------------------------------------------------------
void update(int id) {

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int idx = find_index_by_id(id);
    if (idx < 0) {
        printf("CMS: The record with ID %d does not exist.\n", id);
        return;
    }

    // Save copies of BEFORE and AFTER states for diff & undo
    Student before = arr[idx];
    Student after  = before;

    // Display current record
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    print_student_record(&before);

    // Allow user to edit fields (ENTER == keep old value)
    int changed = 0;

    changed |= prompt_edit_str("Name",
                               after.name, MAX_STR,
                               before.name);

    changed |= prompt_edit_str("Programme",
                               after.programme, MAX_STR,
                               before.programme);

    changed |= prompt_edit_mark(&after.mark,
                                before.mark);

    // If no field changed, abort update
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

    // Confirm if user wants to apply changes
    char confirm[32];
    printf("\nConfirm update (Y/N)? ");

    if (!fgets(confirm, sizeof(confirm), stdin)) {
        printf("Cancelled.\n");
        return;
    }

    // Convert to lowercase to check against 'y'
    for (char *p = confirm; *p; ++p)
        *p = (char)tolower((unsigned char)*p);

    if (confirm[0] != 'y') {
        printf("Cancelled.\n");
        return;
    }

    // Apply changes
    arr[idx] = after;

    printf(GREEN "CMS: Record updated.\n" RESET);

    // Log update details
    audit_log("UPDATE %d | \"%s\" -> \"%s\" | \"%s\" -> \"%s\" | %.1f -> %.1f",
              id,
              before.name, after.name,
              before.programme, after.programme,
              before.mark, after.mark);

    // Prepare undo
    last_op.op = OP_UPDATE;
    last_op.before = before;
    last_op.after  = after;
}



// -----------------------------------------------------------------------------
// FUNCTION: delete
// PURPOSE : Removes a student record from the system.
// PROCESS :
//   1. Look up student by ID
//   2. Display record about to be deleted
//   3. Require user to type ID to confirm
//   4. Remove from array (swap-delete method)
//   5. Write to audit log
//   6. Save undo info
// -----------------------------------------------------------------------------
void delete(int id) {

    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    int i = find_index_by_id(id);
    if (i < 0) {
        printf("CMS: The record with ID %d does not exist.\n", id);
        return;
    }

    // Backup the record so UNDO can restore it
    Student before = arr[i];

    // Show record before deletion
    printf("\n" BOLD "About to delete this record:" RESET "\n");
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    print_student_record(&before);

    // Require exact ID confirmation
    if (!confirm_delete_by_id(before.id)) {
        printf(YELLOW "Cancelled.\n" RESET);
        return;
    }

    // Delete by overwriting this index with last record (O(1))
    arr[i] = arr[arr_size - 1];
    arr_size--;

    printf(GREEN "CMS: Record deleted.\n" RESET);

    audit_log("DELETE %d | \"%s\" | \"%s\" | %.1f",
              before.id, before.name,
              before.programme, before.mark);

    // Prepare undo record
    last_op.op = OP_DELETE;
    last_op.before = before;
}



// -----------------------------------------------------------------------------
// FUNCTION: save
// PURPOSE : Writes the current student array into a .txt file in formatted
//           table form, along with a header containing metadata.
// PROCESS :
//   - Opens output file in write mode
//   - Writes database name + author list
//   - Writes table header
//   - Dumps every student record in formatted columns
//   - Logs the event
// -----------------------------------------------------------------------------
void save() {

    // Guard: Can't save if no records loaded
    if (arr_size == 0) {
        printf("CMS: No records loaded. Use OPEN <filename> first.\n");
        return;
    }

    // Open file for writing (overwrite existing)
    FILE *f = fopen(FILENAME, "w");

    // If cannot open file → fail soft
    if (!f) {
        printf("Save failed.\n");
        return;
    }

    // -----------------------------------------------------
    // 1. Write metadata header
    // -----------------------------------------------------
    fprintf(f, "Database Name: P9_3-CMS\n");
    fprintf(f, "Authors: Ryan, Glenn, Min Han, Jordan, Ben\n");
    fprintf(f, "Table Name: StudentRecords\n\n");

    // -----------------------------------------------------
    // 2. Write column headers
    // -----------------------------------------------------
    fprintf(f, "%-10s %-15s %-25s %-6s\n",
            "ID", "Name", "Programme", "Mark");

    // -----------------------------------------------------
    // 3. Write each student to file
    // -----------------------------------------------------
    for (size_t i = 0; i < arr_size; i++) {
        fprintf(f, "%-10d %-15s %-25s %-6.1f\n",
                arr[i].id,
                arr[i].name,
                arr[i].programme,
                arr[i].mark);
    }

    // Close file
    fclose(f);

    printf("CMS: Saved to \"%s\".\n", FILENAME);

    // Write to log
    audit_log("SAVE %s", FILENAME);
}


// -----------------------------------------------------------------------------
// FUNCTION: summary
// PURPOSE : Displays class-wide statistics including:
//           - total student count
//           - average mark
//           - highest mark + student name
//           - lowest mark + student name
//
// METHOD  :
//   - Iterate through all records once
//   - Track running total, max, min, and indices
// -----------------------------------------------------------------------------
void summary() {

    // If there are no students loaded, nothing to summarize
    if (arr_size == 0) {
        printf("No students available.\n");
        return;
    }

    int total = arr_size;   // Total number of students
    double sum = 0.0;       // Running total of all marks

    // Initialize highest/lowest values using first student's mark
    double highest = arr[0].mark;
    double lowest  = arr[0].mark;
    size_t hi_index = 0;     // Index of student with highest mark
    size_t lo_index = 0;     // Index of student with lowest mark

    // -----------------------------------------------------
    // Scan through all records to compute statistics
    // -----------------------------------------------------
    for (size_t i = 0; i < arr_size; i++) {

        double m = arr[i].mark;
        sum += m;     // Add mark to running sum

        // Track highest mark
        if (m > highest) {
            highest = m;
            hi_index = i;
        }

        // Track lowest mark
        if (m < lowest) {
            lowest = m;
            lo_index = i;
        }
    }

    double average = sum / total;   // Compute class average

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
// SUPPORTED:
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

    // If no operation to undo
    if (last_op.op == OP_NONE) {
        printf(YELLOW "CMS: Nothing to undo.\n" RESET);
        return;
    }

    // Visual header
    printf("\n" BOLD "===== Performing UNDO operation =====" RESET "\n");

    // Print table headers (same style as display)
    printf(BOLD CYAN "%-10s %-20s %-30s %-6s\n" RESET,
           "ID", "Name", "Programme", "Mark");

    printf(CYAN "------------------------------------------------------------------\n" RESET);

    // -------------------------------------------------------------------------
    // CASE 1: Undo INSERT → Remove the record that was inserted
    // -------------------------------------------------------------------------
    if (last_op.op == OP_INSERT) {

        // Locate the inserted record using its ID
        int i = find_index_by_id(last_op.after.id);

        // Only undo if the record is still present
        if (i >= 0 && arr_size > 0) {

            printf(YELLOW "Removed record:\n" RESET);
            print_student_record(&arr[i]);

            // Delete using swap-delete for O(1) removal
            arr[i] = arr[arr_size - 1];
            arr_size--;

            printf(GREEN "CMS: Undo INSERT successful (Record ID %d removed).\n" RESET,
                   last_op.after.id);

            audit_log("UNDO INSERT (ID %d removed)", last_op.after.id);
        }
        else {
            printf(RED "CMS Error: Undo failed. Record ID %d not found.\n" RESET,
                   last_op.after.id);

            audit_log("UNDO INSERT failed (ID %d not found)", last_op.after.id);
        }
    }

    // -------------------------------------------------------------------------
    // CASE 2: Undo DELETE → Re-insert the previously deleted record
    // -------------------------------------------------------------------------
    else if (last_op.op == OP_DELETE) {

        // Ensure enough space in array
        ensure_cap();

        // Reinsert deleted student
        arr[arr_size++] = last_op.before;

        printf(GREEN "Re-inserted record:\n" RESET);
        print_student_record(&last_op.before);

        printf(GREEN "CMS: Undo DELETE successful (Record ID %d re-inserted).\n" RESET,
               last_op.before.id);

        audit_log("UNDO DELETE (ID %d re-inserted)", last_op.before.id);
    }

    // -------------------------------------------------------------------------
    // CASE 3: Undo UPDATE → Restore the BEFORE state
    // -------------------------------------------------------------------------
    else if (last_op.op == OP_UPDATE) {

        // Find the record using ID from the "after" snapshot
        // because the user might have edited the name/programme
        int i = find_index_by_id(last_op.after.id);

        if (i >= 0) {

            printf(YELLOW "Restoring record from state before update:\n" RESET);
            print_student_record(&last_op.before);

            // Restore original version
            arr[i] = last_op.before;

            printf(GREEN "CMS: Undo UPDATE successful (Record ID %d reverted).\n" RESET,
                   last_op.before.id);

            audit_log("UNDO UPDATE (ID %d restored)", last_op.before.id);
        }
        else {

            printf(RED "CMS Error: Undo failed. Record ID %d not found.\n" RESET,
                   last_op.after.id);

            audit_log("UNDO UPDATE failed (ID %d not found)", last_op.after.id);
        }
    }

    // Clear undo history so cannot undo twice
    printf(BOLD "====================================" RESET "\n");

    last_op.op = OP_NONE;
}


/* ---------------------------------------------------- */
/* Command Loop                                         */
/* ---------------------------------------------------- */

// -----------------------------------------------------------------------------
// FUNCTION: main
// PURPOSE : This is the main loop that makes your CMS interactive.
//           It waits for user input, identifies the command,  
//           and calls the appropriate function.
//
// FEATURES:
//   - Reads and parses commands like OPEN, SHOW, INSERT, DELETE, UPDATE, etc.
//   - Supports multi-word commands using sscanf()
//   - Uses strcasecmp() for case-insensitive matching
//   - Runs until user types EXIT
// -----------------------------------------------------------------------------
int main(void) {

    // -------------------------------------------------------------------------
    // Buffers for reading user commands and breaking them into components
    // -------------------------------------------------------------------------
    char userBuffer[256];   // Full line typed by user
    char command[64];       // First word of command
    char arg1[64];          // Optional argument 1
    char arg2[64];          // Optional argument 2
    char arg3[64];          // Optional argument 3

    // -------------------------------------------------------------------------
    // Print current date/time when the system starts
    // -------------------------------------------------------------------------
    time_t now = time(NULL);        // Get current time
    struct tm *t = localtime(&now); // Convert to human readable format

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

    printf("Hello there! P9_3 Classroom Management System [CMS] Ready. Today is %s.\n",
           datetime);
    printf("Type HELP to display available commands.\n");

    // -------------------------------------------------------------------------
    // MAIN COMMAND LOOP
    // This loop runs forever until the user types "EXIT".
    // -------------------------------------------------------------------------
    while (1) {

        // Display the prompt "P9_3>"
        printf("P9_3> ");

        // Read an entire line from the user
        if (!fgets(userBuffer, sizeof(userBuffer), stdin))
            break;  // If input fails (EOF), exit loop

        // Remove trailing newline (so "SHOW\n" becomes "SHOW")
        userBuffer[strcspn(userBuffer, "\n")] = 0;

        // Reset command buffers before parsing
        command[0] = arg1[0] = arg2[0] = arg3[0] = '\0';

        // ---------------------------------------------------------------------
        // Parse up to 4 tokens (command + 3 arguments)
        // Example:
        //    SHOW ALL SORT BY MARK DESC
        // Will map to:
        //    command = "SHOW"
        //    arg1    = "ALL"
        //    arg2    = "SORT"
        //    arg3    = "BY"
        // ---------------------------------------------------------------------
        int n = sscanf(userBuffer, "%63s %63s %63s %63s",
                       command, arg1, arg2, arg3);

        // If user simply pressed ENTER → restart loop
        if (n < 1) continue;

        // ---------------------------------------------------------------------
        // BEGIN COMMAND DISPATCHER
        // ---------------------------------------------------------------------

        // ============================= OPEN =============================
        if (strcasecmp(command, "OPEN") == 0) {

            if (n >= 2)
                open_db(arg1);    // OPEN <filename>
            else
                printf("Usage: OPEN filename\n");
        }

        // ============================= SHOW =============================
        else if (strcasecmp(command, "SHOW") == 0) {

            // Case 1: SHOW ALL
            if (strcasecmp(arg1, "ALL") == 0) {

                // Case 1a: SHOW ALL SORT BY <field> <order>
                if (strcasecmp(arg2, "SORT") == 0 &&
                    strcasecmp(arg3, "BY") == 0) {

                    char field[16] = "\0";
                    char order[16] = "\0";

                    // Extract field + order from the rest of the text input
                    int numOfArgs =
                        sscanf(userBuffer, "%*s %*s %*s %*s %15s %15s",
                               field, order);

                    if (numOfArgs >= 1) {

                        // Convert field and order into uppercase
                        for (int i = 0; field[i]; i++)
                            field[i] = toupper(field[i]);

                        for (int j = 0; order[j]; j++)
                            order[j] = toupper(order[j]);

                        // If order provided → use it
                        if (order[0])
                            showSorted(field, order);
                        else
                            showSorted(field, "ASC");  // default to ASC
                    }
                }
                else {
                    // Simple: SHOW ALL
                    show_all();
                }
            }

            // Case 2: SHOW SUMMARY
            else if (strcasecmp(arg1, "SUMMARY") == 0) {
                summary();
            }

            else {
                printf("Usage: SHOW ALL | SHOW SUMMARY | SHOW ALL SORT BY ...\n");
            }
        }

        // ============================= INSERT =============================
        else if (strcasecmp(command, "INSERT") == 0) {

            Student s;
            char buf[256];

            // Prompt user for new record fields, one-by-one

            // ID
            printf("ID: ");
            if (!fgets(buf, sizeof(buf), stdin)) continue;
            s.id = atoi(buf);

            // Check if ID already exists
            if (query_exists(s.id)) {
                printf("Error: Student with ID %d already exists.\n", s.id);
                continue;
            }

            // Name
            printf("Name: ");
            fgets(buf, sizeof(buf), stdin);
            strtok(buf, "\n");
            strncpy(s.name, buf, MAX_STR - 1);

            // Programme
            printf("Programme: ");
            fgets(buf, sizeof(buf), stdin);
            strtok(buf, "\n");
            strncpy(s.programme, buf, MAX_STR - 1);

            // Mark
            printf("Mark: ");
            fgets(buf, sizeof(buf), stdin);
            s.mark = atof(buf);

            // Insert record into array
            insert_record(s);
        }

        // ============================= QUERY =============================
        else if (strcasecmp(command, "QUERY") == 0) {

            if (n >= 2) {
                int id = atoi(arg1);
                query(id);
            }
            else {
                printf("Usage: QUERY <ID>\n");
            }
        }

        // ============================= UPDATE =============================
        else if (strcasecmp(command, "UPDATE") == 0) {

            if (n >= 2) {
                int id = atoi(arg1);
                update(id);
            }
            else {
                printf("Usage: UPDATE <ID>\n");
            }
        }

        // ============================= DELETE =============================
        else if (strcasecmp(command, "DELETE") == 0) {

            if (n >= 2) {
                int id = atoi(arg1);
                delete(id);
            }
            else {
                printf("Usage: DELETE <ID>\n");
            }
        }

        // ============================= SAVE =============================
        else if (strcasecmp(command, "SAVE") == 0) {

            // SAVE takes no additional arguments
            save();
        }

        // ============================= UNDO =============================
        else if (strcasecmp(command, "UNDO") == 0) {

            undo();
        }

        // ============================= HELP =============================
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

        // ============================= EXIT =============================
        else if (strcasecmp(command, "EXIT") == 0) {

            break;  // leave main loop
        }

        // ============================= UNKNOWN =============================
        else {

            printf("Unknown command. Type HELP to display available commands.\n");
        }
    }

    // Free the dynamic array before exiting
    free(arr);

    // Normal program termination
    return 0;
}



