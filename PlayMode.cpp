#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint room_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > room_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("room.pnct"));
	room_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > room_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("room.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = room_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();
		drawable.min = mesh.min;
		drawable.max = mesh.max;

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = room_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

Load< Sound::Sample > background_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("background.opus"));
});


Load< Sound::Sample > key1_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("sound1.wav"));
});

bool collision(glm::vec3 a_min, glm::vec3 a_max, glm::vec3 b_min, glm::vec3 b_max){
	// Axis aligned bbox check
	if (a_min.x <= b_max.x && a_max.x >= b_min.x &&
		a_min.y <= b_max.y && a_max.y >= b_min.y &&
		a_min.z <= b_max.z && a_max.z >= b_min.z){
		// overlap
		return true;
	}
	return false;
}

PlayMode::PlayMode() : scene(*room_scene) {
	//get pointers for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Ghost") ghost = &transform;
		if (transform.name == "Key1") key1 = &transform;
		if (transform.name == "Lock1") lock1 = &transform;
	}
	if (ghost == nullptr) throw std::runtime_error("Ghost not found.");
	if (key1 == nullptr) throw std::runtime_error("Key1 not found.");
	if (lock1 == nullptr) throw std::runtime_error("Lock1 not found.");
	for (auto & drawable : scene.drawables){
		if (drawable.transform->name == "Ghost"){
			ghost_min = drawable.min;
			ghost_max = drawable.max;
		} else if (drawable.transform->name == "Key1"){
			key1_min = drawable.min;
			key1_max = drawable.max;
		} else if (drawable.transform->name == "Lock1"){
			lock1_min = drawable.min;
			lock1_max = drawable.max;
		}
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	background_loop = Sound::loop(*background_sample, 1.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE){
			// Check for bbox collision and play oneshot
			if (key1 && collision(ghost_min + ghost->position, ghost_max + ghost->position, 
				key1_min + key1->position, key1_max + key1->position)){
				if (key1_oneshot) key1_oneshot->stop();
				key1_oneshot = Sound::play_3D(*key1_sample, 0.3f, key1->position);
			} else if (collision(ghost_min + ghost->position, ghost_max + ghost->position, 
				lock1_min + lock1->position, lock1_max + lock1->position)){
				if (key1_oneshot) key1_oneshot->stop();
				key1_oneshot = Sound::play_3D(*key1_sample, 0.3f, lock1->position);
			}
			return true;
		} else if (evt.key.key == SDLK_M){
			if (!background_muted){ 
				background_loop->set_volume(0.0f);
				background_muted = true;
			} else {
				background_loop->set_volume(1.0f);
				background_muted = false;
			}
			return true;
		} else if (evt.key.key == SDLK_Q){
			if (grabbed_key) grabbed_key = nullptr;
			// Check for collision and grab key
			else if (collision(ghost_min + ghost->position, ghost_max + ghost->position, 
				key1_min + key1->position, key1_max + key1->position)){
				
				grabbed_key = key1;
			}
			return true;
		} else if (evt.key.key == SDLK_E){
			if (!grabbed_key) return true; // Do nothing if not holding a key
			// Unlock lock if key and lock match!
			if (grabbed_key == key1 && collision(ghost_min + ghost->position, ghost_max + ghost->position, 
				lock1_min + lock1->position, lock1_max + lock1->position)){
				// TODO: Play unlocking sound
				locks_left--;
				// Ungrab and remove from view
				grabbed_key = nullptr;
				key1->position = key1->position + glm::vec3(0.0f, 0.0f, -10.0f);
				key1 = nullptr;
			}
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	// ghost->position = ghost->position + glm::vec3(0.0f, 0.9f, dist);

	//move camera:
	{
		//combine inputs into a move:
		constexpr float PlayerSpeed = 10.0f;
		constexpr float CameraSpeed = 10.0f;
		glm::vec2 move = glm::vec2(0.0f);
		glm::vec2 move_camera = glm::vec2(0.0f);
		glm::vec2 move_player = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move_player = glm::normalize(move) * PlayerSpeed * elapsed;
		if (move != glm::vec2(0.0f)) move_camera = glm::normalize(move) * CameraSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		// glm::vec3 up = frame[1];
		// glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move_camera.x * frame_right; // + move_camera.y * frame_forward;

		glm::mat4x3 player = ghost->make_parent_from_local();
		glm::vec3 player_right = player[0];
		glm::vec3 player_forward = player[1];
		ghost->position += move_player.x * player_right + move_player.y * player_forward;

		if (grabbed_key){
			grabbed_key->position = glm::vec3(ghost->position.x, ghost->position.y - 1.0f, grabbed_key->position.z);
		}

	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Locks Left: " + std::to_string(locks_left),
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Locks Left: " + std::to_string(locks_left),
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}
