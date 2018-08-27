#include "null.hpp"

namespace Modules {
namespace Out {

Null::Null(IModuleHost* host)
	: m_host(host) {
	createInput(this);
}

void Null::process(Data) {
}

}
}
