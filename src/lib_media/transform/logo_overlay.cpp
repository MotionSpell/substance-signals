#include "logo_overlay.hpp"

#include <lib_modules/modules.hpp>
#include <lib_media/common/picture.hpp>
#include <lib_modules/utils/loader.hpp>
#include <algorithm> //std::min
#include <cstring> // memcpy

using namespace Modules;

namespace {

static uint8_t blend(uint8_t a, uint8_t b, int alpha) {
	return (alpha  * b + (256 - alpha) * a) >> 8;
}

static void compose(const DataPicture* pic,
    int x, int y,
    const DataPicture* overlay,
    const DataPicture* mask) {
	auto const picRes = pic->getFormat().res;
	auto const logoRes = overlay->getFormat().res;
	auto const width = std::min<int>(logoRes.width, picRes.width - x);
	for (int p = 0; p < (int)pic->getNumPlanes(); ++p) {
		auto const divisor = int(pic->getStride(0) / pic->getStride(p));
		auto const multiplicator = (int)pic->getStride(p) > picRes.width ? int(pic->getStride(p)/picRes.width) : 1;
		auto planePic = const_cast<DataPicture*>(pic)->getPlane(p); //TODO: have modules able to write in-place
		auto planeLogo = overlay->getPlane(p);
		auto const picPitch = pic->getStride(p);
		auto const logoPitch = overlay->getStride(p);
		auto const yAdj = (y / divisor);
		auto const logoResHDiv = logoRes.height / divisor;
		auto const picResHDiv = picRes.height / divisor;
		auto const xAdj = x * multiplicator / divisor;
		auto dst = planePic + yAdj * picPitch + xAdj;
		auto const blitHeight = std::min(logoResHDiv, picResHDiv - yAdj);
		auto const blitWidth = width * multiplicator / divisor;
		if (mask) {
			auto const rgbaStride = (divisor * 4) / multiplicator;
			auto const maskPitch = (divisor * mask->getStride(0)) / multiplicator;
			auto const logoAlphaP0 = mask->getPlane(0);
			for (int h = 0; h < blitHeight; ++h) {
				auto const maskLine = logoAlphaP0 + h * maskPitch;
				auto const logoLine = planeLogo + h * logoPitch;
				for (int w = 0; w < blitWidth; ++w) {
					const auto alpha = maskLine[w * rgbaStride + 3] + 1;
					dst[w] = blend(dst[w], logoLine[w], alpha);
				}
				dst += picPitch;
			}
		} else {
			for (int h = 0; h < blitHeight; ++h) {
				memcpy(dst, planeLogo, blitWidth);
				planeLogo += logoPitch;
				dst += picPitch;
			}
		}
	}
}

class LogoOverlay : public Module {
	public:
		LogoOverlay(KHost*, LogoOverlayConfig* cfg)
			: m_cfg(*cfg) {
			m_mainInput = addInput();
			m_overlayInput = addInput();
			m_output = addOutput<OutputDefault>();
		}
		void process() {
			Data data;
			if (m_mainInput->tryPop(data)) {
				processVideo(safe_cast<const DataPicture>(data));
			} else if (m_overlayInput->tryPop(data)) {
				processOverlay(safe_cast<const DataPicture>(data));
			} else
				throw error("Awaken on unknown input.");
		}

	private:
		const LogoOverlayConfig m_cfg;

		std::shared_ptr<const DataPicture> m_overlay;
		std::shared_ptr<const DataPicture> m_convertedOverlay;
		std::shared_ptr<const DataPicture> m_overlayMask;

		KOutput* m_output;
		KInput* m_mainInput;
		KInput* m_overlayInput;

		void processOverlay(std::shared_ptr<const DataPicture> overlay) {
			m_overlay = overlay;
		}
		void processVideo(std::shared_ptr<const DataPicture> pic) {
			if (!m_convertedOverlay && m_overlay) {
				createOverlay(pic->getFormat().format);
				m_overlay = nullptr;
			}

			if (m_convertedOverlay)
				compose(pic.get(), m_cfg.x, m_cfg.y, m_convertedOverlay.get(), m_overlayMask.get());

			m_output->post(pic);
		}

		void createOverlay(PixelFormat pixelFormat) {
			struct OutStub : ModuleS {
				std::function<void(Data)> fn;
				void processOne(Data data) override {
					fn(data);
				}
			};

			auto res = m_cfg.dim;
			{
				const PictureFormat pfo(res, pixelFormat);
				OutStub stub;
				stub.fn = [this](Data data) {
					this->m_convertedOverlay = safe_cast<const DataPicture>(data);
				};
				auto converter = loadModule("VideoConvert", &NullHost, &pfo);
				converter->getOutput(0)->connect(stub.getInput(0));
				converter->getInput(0)->push(m_overlay);
				converter->process();
				converter->flush();
			}
			if (m_overlay->getFormat().hasTransparency()) {
				OutStub stub;
				stub.fn = [this](Data data) {
					this->m_overlayMask = safe_cast<const DataPicture>(data);
				};
				const PictureFormat pfo(res, m_overlay->getFormat().format);
				auto converter = loadModule("VideoConvert", &NullHost, &pfo);
				converter->getOutput(0)->connect(stub.getInput(0));
				converter->getInput(0)->push(m_overlay);
				converter->process();
				converter->flush();
			}
		}
};

IModule* createObject(KHost* host, void* va) {
	auto config = (LogoOverlayConfig*)va;
	enforce(host, "LogoOverlay: host can't be NULL");
	enforce(config, "LogoOverlay: config can't be NULL");
	return createModule<LogoOverlay>(host, config).release();
}

auto const registered = Factory::registerModule("LogoOverlay", &createObject);

}

