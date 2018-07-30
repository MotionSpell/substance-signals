#include "null.hpp"

namespace Modules {
namespace Out {

Null::Null() {
	createInput(this);
}

void Null::process(Data) {
}

}
}
