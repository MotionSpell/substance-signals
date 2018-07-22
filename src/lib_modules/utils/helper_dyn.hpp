#pragma once

#include "helper.hpp"

namespace Modules {

//dynamic input number specialized module
//note: ports added automatically will carry the DataLoose type which doesn't
//      allow to perform all safety checks ; consider adding ports manually if
//      you can
class ModuleDynI : public Module {
	public:
		ModuleDynI() = default;
		virtual ~ModuleDynI() noexcept(false) {}

		IInput* addInput(IInput *p) { //takes ownership
			bool isDyn = false;
			std::unique_ptr<IInput> pEx;
			if (inputs.size() && dynamic_cast<DataLoose*>(inputs.back().get())) {
				isDyn = true;
				pEx = std::move(inputs.back());
				inputs.pop_back();
			}
			inputs.push_back(uptr(p));
			if (isDyn)
				inputs.push_back(std::move(pEx));
			return p;
		}
		int getNumInputs() const override {
			if (inputs.size() == 0)
				return 1;
			else if (inputs[inputs.size() - 1]->getNumConnections() == 0)
				return (int)inputs.size();
			else
				return (int)inputs.size() + 1;
		}
		IInput* getInput(int i) override {
			if (i == (int)inputs.size())
				addInput(new Input<DataLoose>(this));
			else if (i > (int)inputs.size())
				throw std::runtime_error(format("Incorrect port number %s for dynamic input.", i));

			return inputs[i].get();
		}
		std::vector<int> getInputs() const {
			std::vector<int> r;
			for (int i = 0; i < getNumInputs() - 1; ++i)
				r.push_back(i);
			return r;
		}

};

}
