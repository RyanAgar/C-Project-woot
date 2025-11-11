# P9_3-INF1002-C-Project
This project was developed by Group P9_3 for INF1002.  
It is a student record management system written in C, featuring:

- Core operations: OPEN, SHOW, SORT, INSERT, QUERY, UPDATE, DELETE, SAVE, SUMMARY
- Unique features: UNDO and Audit Logging
- Dynamic array management with resizing
- Colorized CLI output for marks (green = excellent, yellow = average, red = failing)

---

## Design Decisions

- **Delete operation:** We chose to swap the record to be deleted with the last element in the array.  
  This makes deletion O(1) and avoids shifting all elements.
- **Undo feature:** Implemented by storing the “before” and “after” states of the last operation.  
  This was inspired by version control systems and ensures transparency.
- **Audit log:** Every operation writes to `P9_3-CMS.log` with a timestamp and user context.  
  This was added to make the system accountable and traceable.
- **Parsing:** We wrote a custom parser (`parse_line`) that tolerates variable spacing and tabs.  
  This was necessary because our input files weren’t always consistently formatted.
- **Sorting:** We used `qsort` with custom comparators for ID and mark.  
  We deliberately avoided writing our own sort to reduce bugs and leverage the standard library.

---

## Human Authorship Notes

This code was written collaboratively by our group.  
Some quirks that reflect our process:
- We skip the first 5 lines of the input file in `open_db()` because our dataset had metadata headers.
- We added ANSI color codes for fun, to make the CLI more engaging.
- Our comments are tailored to our assignment requirements and group workflow, not generic boilerplate.
- The undo logic reflects our brainstorming sessions — we debated whether to support multiple undos, but settled on a single step for simplicity.

---

## Contributors
- Ryan
- Glenn
- Min Han
- Jordan
- Ben

---

## How To Run The Program
Open bash:
gcc -o P9_3_CMS P9_3_CMS.c
./P9_3_CMS.exe
