#include "print.hpp"

namespace Modules {
namespace Out {

void Print::process(Data data_)  {
	auto data = safe_cast<const DataBase>(data_);
	os << "Print: Received data of size: " << data->data().len << std::endl;
}

Print::Print(IModuleHost* host, std::ostream &os)
	: m_host(host), os(os) {
	createInput(this);
}

}
}
