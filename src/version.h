/***********************************************************************************************************************************
Version Numbers and Names
***********************************************************************************************************************************/
#ifndef VERSION_H
#define VERSION_H

/***********************************************************************************************************************************
Official name of the project
***********************************************************************************************************************************/
#define PROJECT_NAME                                                "pgBackRest"

/***********************************************************************************************************************************
Standard binary name
***********************************************************************************************************************************/
#define PROJECT_BIN                                                 "pgbackrest"

/***********************************************************************************************************************************
Config file name. The path will vary based on configuration.
***********************************************************************************************************************************/
#define PROJECT_CONFIG_FILE                                         PROJECT_BIN ".conf"

/***********************************************************************************************************************************
Config include path name. The parent path will vary based on configuration.
***********************************************************************************************************************************/
#define PROJECT_CONFIG_INCLUDE_PATH                                 "conf.d"

/***********************************************************************************************************************************
Format Number -- defines format for info and manifest files as well as on-disk structure. If this number changes then the repository
will be invalid unless migration functions are written.
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT                                           5

/***********************************************************************************************************************************
Software version
***********************************************************************************************************************************/
#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       55
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"

#define PROJECT_VERSION                                                                                                            \
    STRINGIFY(PROJECT_VERSION_MAJOR) "." STRINGIFY(PROJECT_VERSION_MINOR) "." STRINGIFY(PROJECT_VERSION_PATCH)                     \
    PROJECT_VERSION_SUFFIX

#define PROJECT_VERSION_NUM                                                                                                        \
    ((unsigned int)((PROJECT_VERSION_MAJOR * 1000000) + (PROJECT_VERSION_MINOR * 1000) + PROJECT_VERSION_PATCH))

#endif
