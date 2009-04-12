/*
 * This is the EDJS error message file.
 *
 * The format for each JS error message is:
 *
 * MSG_DEF(<SYMBOLIC_NAME>, <ERROR_NUMBER>, <ARGUMENT_COUNT>, <EXCEPTION_NAME>,
 *         <FORMAT_STRING>)
 *
 * where ;
 * <SYMBOLIC_NAME> is a legal C identifer that will be used in the
 * JS engine source.
 *
 * <ERROR_NUMBER> is an unique integral value identifying this error.
 *
 * <ARGUMENT_COUNT> is an integer literal specifying the total number of
 * replaceable arguments in the following format string.
 *
 * <EXCEPTION_NAME> is an exception index from the enum in jsexn.c;
 * JSEXN_NONE for none.  The given exception index will be raised by the
 * engine when the corresponding error occurs.
 *
 * <FORMAT_STRING> is a string literal, optionally containing sequences
 * {X} where X  is an integer representing the argument number that will
 * be replaced with a string value when the error is reported.
 *
 * e.g.
 *
 * MSG_DEF(JSMSG_NOT_A_SUBSPECIES, 73, JSEXN_NONE, 2,
 *         "{0} is not a member of the {1} family")
 *
 * can be used:
 *
 * JS_ReportErrorNumber(JSMSG_NOT_A_SUBSPECIES, "Rhino", "Monkey");
 *
 * to report:
 *
 * "Rhino is not a member of the Monkey family"
 *
 * Before adding a new MSG_DEF at the end, look for JSMSG_UNUSED<n> free
 * index placeholders in the middle of the list.
 */

//000 - errors that are specific to edjs internals and theoretically should never happen
MSG_DEF(EDJSERR_NOT_AN_ERROR,            0,   0, JSEXN_NONE,        "<Error #0 is reserved>")
MSG_DEF(EDJSERR_UNKNOWN_ERROR,           1,   0, JSEXN_INTERNALERR, "An unknown error has occured")
MSG_DEF(EDJSERR_PRIVATE_DATA_NOT_SET,    2,   0, JSEXN_INTERNALERR, "Failed to retrieve internal private data")
MSG_DEF(EDJSERR_COOKIE_UNSET,            3,   0, JSEXN_INTERNALERR, "Unable to access cookie data")
MSG_DEF(EDJSERR_DISK_IO,                 4,   0, JSEXN_INTERNALERR, "Failed to read data from disk")
MSG_DEF(EDJSERR_OUT_OF_MEMORY,           5,   0, JSEXN_INTERNALERR, "Failed to malloc memory")
MSG_DEF(EDJSERR_COMPILING_SETTINGS,      6,   0, JSEXN_INTERNALERR, "Failed to compile settings file")
MSG_DEF(EDJSERR_SETTINGS_NOT_OBJ,        7,   0, JSEXN_INTERNALERR, "Settings script did not return object")
MSG_DEF(EDJSERR_COMPILING_CORE,          8,   0, JSEXN_INTERNALERR, "Failed to compile core file")
MSG_DEF(EDJSERR_EXECUTION_TIME_EXCEEDED, 9,   0, JSEXN_INTERNALERR, "Max execution time exceed")

//100 - errors that are specific to js engine failures
MSG_DEF(EDJSERR_CREATING_RUNTIME,        101, 0, JSEXN_INTERNALERR, "Failed to create new runtime")
MSG_DEF(EDJSERR_CREATING_CONTEXT,        102, 0, JSEXN_INTERNALERR, "Failed to create new context")
MSG_DEF(EDJSERR_COMPILING_SCRIPT,        103, 0, JSEXN_INTERNALERR, "Failed to compile script")
MSG_DEF(EDJSERR_EXECUTING_SCRIPT,        104, 0, JSEXN_INTERNALERR, "Failed to execute script")
MSG_DEF(EDJSERR_INITING_STANDARD_CLASSES,105, 0, JSEXN_INTERNALERR, "Failed to initialize standard javascript classes")
MSG_DEF(EDJSERR_INITING_EDJS_CLASSES,    106, 0, JSEXN_INTERNALERR, "Failed to initialize edjs javascript classes")

MSG_DEF(EDJSERR_CREATING_STRING,         107, 0, JSEXN_INTERNALERR, "Failed to create new string object")
MSG_DEF(EDJSERR_CREATING_SCRIPT_OBJ,     108, 0, JSEXN_INTERNALERR, "Failed to create new script object")
MSG_DEF(EDJSERR_CREATING_ARRAY,          109, 0, JSEXN_INTERNALERR, "Failed to create new array object")
MSG_DEF(EDJSERR_CREATING_OBJECT,         110, 0, JSEXN_INTERNALERR, "Failed to create new object")
MSG_DEF(EDJSERR_CREATING_NUMBER,         111, 0, JSEXN_INTERNALERR, "Failed to create new number")

MSG_DEF(EDJSERR_HAS_PROPERTY,            112, 2, JSEXN_INTERNALERR, "Failed to determine property existance of {1} on {0}")
MSG_DEF(EDJSERR_GET_PROPERTY,            113, 2, JSEXN_INTERNALERR, "Failed to retrieve property {1} on {0}")
MSG_DEF(EDJSERR_SET_PROPERTY,            114, 2, JSEXN_INTERNALERR, "Failed to set property {1} on {0}")
MSG_DEF(EDJSERR_ARRAY_RETRIEVAL,         115, 0, JSEXN_INTERNALERR, "Failed to retrieve array value")
MSG_DEF(EDJSERR_ARRAY_LEN,               116, 0, JSEXN_INTERNALERR, "Failed to determine array length")
MSG_DEF(EDJSERR_ADD_ROOT,                117, 0, JSEXN_INTERNALERR, "Failed to obtain local root for variable")
MSG_DEF(EDJSERR_ENUMERATE_OBJ,           118, 1, JSEXN_INTERNALERR, "Failed to enumerate object {0}")
MSG_DEF(EDJSERR_CONSTRUCTING_OBJ,        119, 0, JSEXN_INTERNALERR, "Failed to construct new object")
MSG_DEF(EDJSERR_GET_GETTERS_SETTERS,     120, 0, JSEXN_INTERNALERR, "Failed to retriever getters and settings for property")

//200 - errors that imply invalid arguments or function use
MSG_DEF(EDJSERR_NUM_ARGS,                201, 1, JSEXN_INTERNALERR, "Wrong number of arguments specified for {0}")
MSG_DEF(EDJSERR_VAR_TYPE,                202, 2, JSEXN_INTERNALERR, "Invalid property type for {0}. Expected {1}")
MSG_DEF(EDJSERR_FILENAME_LENGTH,         203, 1, JSEXN_INTERNALERR, "Supplied file name too long: {0}")
MSG_DEF(EDJSERR_FILENAME_LOOP,           204, 1, JSEXN_INTERNALERR, "Symbolic link loop: {0}")
MSG_DEF(EDJSERR_FILENAME_RESOLVE,        205, 1, JSEXN_INTERNALERR, "File could not be resolved: {0}")

//300 - errors relating to type conversion
MSG_DEF(EDJSERR_ID_TO_VAL,               301, 0, JSEXN_INTERNALERR, "Failed to convert id to value")
MSG_DEF(EDJSERR_OBJECT_CLASS,            302, 0, JSEXN_INTERNALERR, "Could not determine class for object")
MSG_DEF(EDJSERR_INT_CONVERSION,          303, 0, JSEXN_INTERNALERR, "Could not convert value to integer")
MSG_DEF(EDJSERR_STRING_CONVERSION,       304, 0, JSEXN_INTERNALERR, "Could not convert value to string")
MSG_DEF(EDJSERR_OBJECT_CONVERSION,       305, 0, JSEXN_INTERNALERR, "Could not convert value to object")

//400 - errors specific to module importing
MSG_DEF(EDJSERR_LOCATING_MODULE,         401, 1, JSEXN_INTERNALERR, "Failed to find module {0}")
MSG_DEF(EDJSERR_OPENING_MODULE,          402, 1, JSEXN_INTERNALERR, "Failed to open module {0}")
MSG_DEF(EDJSERR_READING_MODULE,          403, 1, JSEXN_INTERNALERR, "Failed to read module {0}")
MSG_DEF(EDJSERR_COMPILING_MODULE,        404, 1, JSEXN_INTERNALERR, "Failed to compile module {0}")
MSG_DEF(EDJSERR_LOCATING_MODULE_INIT,    405, 1, JSEXN_INTERNALERR, "Failed to locate init function in module {0}")
MSG_DEF(EDJSERR_INITING_MODULE,          406, 1, JSEXN_INTERNALERR, "Failed to initialize module {0}")
MSG_DEF(EDJSERR_IMPORTING_MODULE_PROP,   407, 2, JSEXN_INTERNALERR, "Module {0} has no property {1}")
MSG_DEF(EDJSERR_MODULE_CHAIN_TYPE,       408, 1, JSEXN_INTERNALERR, "Existing property in module package chain is not object: {0}")







