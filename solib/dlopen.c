#include <dlfcn.h>
#include <stdio.h>

typedef void (*hello_t)();
int main() {
    void * lib = dlopen("test.so", RTLD_LAZY);
    if(lib == NULL) {
        printf("dlopen failed_%s\n", dlerror());
        return 1;
    }
    hello_t hello = (hello_t)dlsym(lib, "hello");
    int * a1 = (int *)dlsym(lib, "a");
    printf("%d\n", *a1);
    hello();
    dlclose(lib);
    return 0;
}
