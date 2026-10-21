#include "../src/auth/auth.h"

#include <cassert>

int main()
{
    const std::string password_hash = auth::hashPassword("correct horse battery staple");
    assert(password_hash != "correct horse battery staple");
    assert(auth::verifyPassword("correct horse battery staple", password_hash));
    assert(!auth::verifyPassword("wrong password", password_hash));
    assert(auth::verifyPassword("@24Dinesh", "pbkdf2_sha256$210000$99b8e38a15d9b9ef73dff7093b6d48a3$b967d9b28025edb7f722e7a69fce6075cab8460411bb5c544f37f6116ee862f0"));

    const std::string token = auth::createToken();
    assert(token.size() == 64);
    assert(auth::hashToken(token) != token);
    return 0;
}
