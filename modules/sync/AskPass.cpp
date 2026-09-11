#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

int main()
{
    const auto *value = std::getenv("OMAWEB_SYNC_CREDENTIAL_FD");
    if (!value) {
        return 1;
    }
    char *end = nullptr;
    const auto descriptor = std::strtol(value, &end, 10);
    if (!end || *end != '\0' || descriptor < 0) {
        return 1;
    }
    char buffer[256];
    while (true) {
        const auto readBytes = read(static_cast<int>(descriptor), buffer, sizeof(buffer));
        if (readBytes == 0) {
            break;
        }
        if (readBytes < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 1;
        }
        if (std::fwrite(buffer, 1, static_cast<size_t>(readBytes), stdout)
            != static_cast<size_t>(readBytes)) {
            return 1;
        }
    }
    return std::fputc('\n', stdout) == EOF ? 1 : 0;
}
