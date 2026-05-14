
#include <stdio.h>
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main() {
    printf("Value: %s\n", FAP_APP_VERSION);
    printf("Stringified: %s\n", TOSTRING(FAP_APP_VERSION));
    return 0;
}
