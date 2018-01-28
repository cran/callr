
#ifdef _WIN32

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <Rdefines.h>
#include <windows.h>


SEXP callr_is_named_pipe_open(SEXP pipe_ext) {
    if (pipe_ext == R_NilValue)
        error("Not a named pipe handle.");

    // This function currently only tests if the named pipe has been closed
    // "properly", by callr_close_named_pipe(). It doesn't test if the
    // other end of the pipe has been closed.

    if (R_ExternalPtrAddr(pipe_ext) == NULL)
        return ScalarLogical(0);

    return ScalarLogical(1);
}


SEXP callr_close_named_pipe(SEXP pipe_ext) {
    if (pipe_ext == R_NilValue || R_ExternalPtrAddr(pipe_ext) == NULL)
        return R_NilValue;

    HANDLE h = (HANDLE)R_ExternalPtrAddr(pipe_ext);
    DisconnectNamedPipe(h);
    CloseHandle(h);
    R_ClearExternalPtr(pipe_ext);

    return R_NilValue;
}


// For the finalizer, we need to wrap the SEXP function with a void function.
void named_pipe_finalizer(SEXP pipe_ext) {
    callr_close_named_pipe(pipe_ext);
}


SEXP callr_create_named_pipe(SEXP name, SEXP mode) {
    if (!isString(name) || name == R_NilValue || length(name) != 1) {
        error("`name` must be a character vector of length 1.");
    }

    if (!isString(mode) || mode == R_NilValue || length(mode) != 1) {
        error("`mode` must be either 'w' or 'r'.");
    }

    const char* name_str = CHAR(STRING_ELT(name, 0));
    // const char* mode_str = CHAR(STRING_ELT(mode, 0));


    if (strncmp("\\\\.\\pipe\\", name_str, sizeof("\\\\.\\pipe\\") - 1) != 0) {
        error("`name` must start with \"\\\\.\\pipe\\\"");
    }

    // int mode_num;
    // if (strcmp(mode_str, "r") == 0)
    //     mode_num = 0;
    // else if (strcmp(mode_str, "w") == 0)
    //     mode_num = 1;
    // else
    //     error("`mode` must be either 'w' or 'r'.");


    HANDLE hPipe = CreateNamedPipe(
        name_str,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE |       // message type pipe
        PIPE_READMODE_MESSAGE |   // message-read mode
        PIPE_REJECT_REMOTE_CLIENTS |
        PIPE_NOWAIT,              // blocking mode
        1,                        // max. instances
        1024,                     // output buffer size
        1024,                     // input buffer size
        0,                        // client time-out
        NULL                      // default security attribute
    );


    if (hPipe == INVALID_HANDLE_VALUE) {
        error("Error creating named pipe. Error %d.",
              (int)GetLastError());
    }

    // Wrap it in an external pointer
    SEXP pipe_ext = PROTECT(R_MakeExternalPtr(hPipe, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(pipe_ext, named_pipe_finalizer, TRUE);
    UNPROTECT(1);
    return pipe_ext;
}

    
SEXP callr_write_named_pipe(SEXP pipe_ext, SEXP text) {
    if (!isString(text) || text == R_NilValue || length(text) != 1) {
        error("`text` must be a character vector of length 1.");
    }

    if (pipe_ext == R_NilValue) {
        error("Pipe must not be NULL.");
    }

    HANDLE hPipe = (HANDLE) R_ExternalPtrAddr(pipe_ext);
    if (hPipe == NULL) {
        error("Pipe handle is NULL.");
    }


    const char* text_str = CHAR(STRING_ELT(text, 0));

    DWORD n_written;
    BOOL success = WriteFile(
        hPipe,
        text_str,
        strlen(text_str),
        &n_written,
        NULL
    );

    if (!success || strlen(text_str) != n_written) {

        DWORD last_error = GetLastError();
        const char* extra_info = "";
        if (last_error == 536) {
            extra_info = " No process is listening on other end of pipe.";
        }

        error("An error occurred when writing to the named pipe. Error %d.%s",
            (int)last_error, extra_info);
    }

    FlushFileBuffers(hPipe);

    return text;
}

#endif
