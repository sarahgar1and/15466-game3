#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	int locks_left = 1;
 
	Scene::Transform *ghost = nullptr;
	Scene::Transform *key1 = nullptr;
	Scene::Transform *lock1 = nullptr;
	Scene::Transform *grabbed_key = nullptr;
	// Player bbox
	glm::vec3 ghost_min;
	glm::vec3 ghost_max;
	// Key bbox
	glm::vec3 key1_min;
	glm::vec3 key1_max;
	// Lock bbox
	glm::vec3 lock1_min;
	glm::vec3 lock1_max;

	float wobble = 0.0f;
	
	std::shared_ptr< Sound::PlayingSample > background_loop;
	bool background_muted = false;
	std::shared_ptr< Sound::PlayingSample > key1_oneshot;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
