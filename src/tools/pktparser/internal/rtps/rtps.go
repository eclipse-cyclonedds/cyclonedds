package rtps

import ()

func Setup() {
	// do this in init() ?
	defaultSession.init()
}

func Start() {
	defaultSession.start()
}
