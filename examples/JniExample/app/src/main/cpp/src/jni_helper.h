#ifndef NELEM
# define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
#endif

#define NATIVE_METHOD(className, functionName, signature) \
    { #functionName, signature, reinterpret_cast<void*>(className ## _ ## functionName) }