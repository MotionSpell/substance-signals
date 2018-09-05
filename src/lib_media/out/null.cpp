#include "null.hpp"

namespace Modules {
namespace Out {

Null::Null(IModuleHost* host)
	: m_host(host) {
	(void)m_host;
	addInput(this);
}

void Null::process(Data) {
}

}
}
