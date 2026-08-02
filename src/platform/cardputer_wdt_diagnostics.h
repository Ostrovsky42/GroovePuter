#pragma once

// Prints the task/core captured by the task-watchdog ISR before the previous
// reset. The record is cleared after it is reported.
void printAndClearCardputerWdtDiagnostic();
