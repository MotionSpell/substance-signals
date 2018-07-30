#include "null.hpp"

namespace Modules {
namespace Out {

Null::Null() {
	addInput(new Input(this));
}

void Null::process(Data data) {
	(void)data;
}

}
}
