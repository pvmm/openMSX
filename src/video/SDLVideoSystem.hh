// $Id$

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Reactor;
class Display;
class RenderSettings;
class VisibleSurface;
class Layer;
class Setting;

class SDLVideoSystem : public VideoSystem, private EventListener,
                       private Observer<Setting>, private noncopyable
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	explicit SDLVideoSystem(Reactor& reactor);

	/** Deactivates this video system.
	  */
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual bool prepare();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);
	virtual void setWindowTitle(const std::string& title);

private:
	// EventListener
	void signalEvent(const Event& event);
	// Observer
	void update(const Setting& subject);

	void getWindowSize(unsigned& width, unsigned& height);
	void resize();

	Reactor& reactor;
	Display& display;
	RenderSettings& renderSettings;
	std::auto_ptr<VisibleSurface> screen;
	std::auto_ptr<Layer> console;
	std::auto_ptr<Layer> snowLayer;
	std::auto_ptr<Layer> iconLayer;
};

} // namespace openmsx

#endif
