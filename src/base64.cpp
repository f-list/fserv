#include "precompiled_headers.hpp"

#include "base64.hpp"
#include <string.h>
#include "modp_b64.hpp"


// This is taken from the chromium source base, and is covered by the license in 3rdparty/LICENSE.chromium
namespace thirdparty {

    bool Base64Encode(std::string& input, std::string& output) {
        std::string temp;
        temp.resize(modp_b64_encode_len(input.size())); // makes room for null byte

        // null terminates result since result is base64 text!
        int input_size = static_cast<int> (input.size());
        int output_size = modp_b64_encode(&(temp[0]), input.data(), input_size);
        if (output_size < 0)
            return false;

        temp.resize(output_size); // strips off null byte
        output.swap(temp);
        return true;
    }

}
