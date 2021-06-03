#ifndef __axz_export__
#define __axz_export__

#ifdef _WIN32
  #ifdef AXZDICT_EXPORT
    #define AXZDICT_DECLSPEC __declspec(dllexport)
  #else
    #define AXZDICT_DECLSPEC __declspec(dllimport)
  #endif
  /**
   *  @brief Defines the export/import macro for class declarations
   */
  #define AXZDICT_DECLCLASS AXZDICT_DECLSPEC
#else
   /**
    *  @brief Defines export function names of the API
    */
  #define AXZDICT_DECLSPEC __attribute__ ((visibility ("default")))

  /**
   *  @brief Defines the export/import macro for class declarations
   */
  #define AXZDICT_DECLCLASS AXZDICT_DECLSPEC
#endif

#endif
