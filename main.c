#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include <raylib.h>
#include <raymath.h>

#include "object.h"

#ifdef PLATFORM_WEB
    #include <emscripten/emscripten.h>
#endif

#define FPS 0 // 0 means no cap
#define WIDTH  800
#define HEIGHT 600

// Playable area
#define AREA_W 4000
#define AREA_H 4000

// Player
#define PLAYER_ACCEL 600.0
#define PLAYER_DEACCEL 300.0
#define PLAYER_VEL_CAP 600.0
#define PLAYER_ROT_SPEED 3.5
#define PLAYER_RADIUS 15 // Radius for collision checking
#define PLAYER_SIZE    7 // Actual size of the triangle
#define PLAYER_SHOOT_DELAY 0.15
#define PLAYER_HEALTH 5
#define PLAYER_HEALTH_MAX 10
#define PLAYER_INVUL_SEC 0.5 // Time between hits
#define PLAYER_KNOCKBACK 100 // Knockback from getting hit

// Asteroid
#define ASTEROID_COUNT_BASE 15
#define ASTEROID_COUNT_INCR 5 // increment

#define ASTEROID_MIN_VERTS  7
#define ASTEROID_MAX_VERTS 12

#define ASTEROID_MIN_SIZE  40
#define ASTEROID_MAX_SIZE 100
#define ASTEROID_DESTROY_SIZE 20

#define ASTEROID_MIN_VEL 150.0
#define ASTEROID_MAX_VEL 300.0
#define ASTEROID_VEL_SCALE_FACTOR 20 // higher = size matters less

#define ASTEROID_DISTORTION 15

#define ASTEROID_ROT_SPEED 30.0

#define ASTEROID_MAX_HEALTH 5
#define ASTEROID_MIN_HEALTH 1

// Projectile
#define PROJECTILE_RADIUS 2
#define PROJECTILE_OFFSET 10
#define PROJECTILE_VEL 800.0
#define PROJECTILE_LIFETIME 0.35
#define ENEMY_PROJ_VEL 200.0
#define ENEMY_PROJ_LIFETIME 2.0
#define PROJECTILE_HEALTH 1

// Enemy Base
#define BASE_RADIUS 35
#define BASE_SIDES 8
#define BASE_HEALTH 15
#define BASE_SHOOT_DELAY 2.0 // in seconds

// Object types
#define TYPE_PLAYER     0
#define TYPE_ASTEROID   1
#define TYPE_PROJECTILE 2
#define TYPE_ENEMY_PROJ 3
#define TYPE_BASE       4

// Object layers
#define LAYER_PLAYER     1<<0
#define LAYER_ASTEROID   1<<1
#define LAYER_PROJECTILE 1<<2
#define LAYER_ENEMY_PROJ 1<<3
#define LAYER_BASE       1<<4

// Enemy base indicator arrows
#define ARROW_MAX_RADIUS 10
#define ARROW_DISTANCE   45 // Distance to the player
//

#define NO_ASTEROID_RADIUS 130 // Radius around the player where asteroids can't spawn
#define NO_LIFETIME -1
#define STAR_FACTOR 5000 // Chance to get stars (1/STAR_FACTOR)
#define FONT_SIZE 20

// First element of list of all objects
Node* objs_head = NULL;

Object* player = NULL;
clock_t lastShoot;
clock_t lastHit;

int level = 0;
int highscore = 0;

Camera2D camera;

Vector2* basesPos = NULL; // Positions of the bases

Texture2D starsTex;

clock_t lastBaseShoot;

void OnInterrupt(int signal) {
	puts("\nProgram terminated by SIGINT. Exiting.");
	exit(EXIT_SUCCESS);
}

// Reimplementing GenImageWhiteNoise to have ratio smaller than 0.01f
void GenerateStars() {
	// Generating image
	Color* pixels = malloc(AREA_W * AREA_H * sizeof(Color));
	
	for (int i = 0; i < AREA_W*AREA_H; ++i) {
		if (GetRandomValue(1, STAR_FACTOR) > 1) pixels[i] = BLACK;
		else pixels[i] = WHITE;
	}

	Image starImg = {
		.data = pixels,
		.width  = AREA_W,
		.height = AREA_H,
		.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
		.mipmaps = 1,
	};

	// Getting texture
	starsTex = LoadTextureFromImage(starImg);

	// Freeing image
	free(pixels);
}

void FreeObjects() {
	Node* node = objs_head;
	while (node != NULL) {
		Node* next = node->next;
		FreeNode(node);
		node = next;
	}

	objs_head = NULL;
	player = NULL;
}

void FreeBasesPos() {
	if (basesPos) free(basesPos);
}

void FreeStarsTex() {
	UnloadTexture(starsTex);
}

void OneTimeInit() {
	signal(SIGINT, OnInterrupt);

	// Window stuff
	SetTraceLogLevel(LOG_WARNING); /* getting rid of annoying init info */
	InitWindow(WIDTH, HEIGHT, "asteroids :3");
	atexit(CloseWindow);

	// Generating stars texture
	GenerateStars();
	atexit(FreeStarsTex);

	// Other frees
	atexit(FreeObjects);
	atexit(FreeBasesPos);

	// Variables
	lastShoot = clock();
	lastHit   = clock();
	lastBaseShoot = clock();

	camera = (Camera2D){
		.offset = (Vector2){WIDTH/2, HEIGHT/2},
		.target = (Vector2){WIDTH/2, HEIGHT/2},
		.rotation = 0,
		.zoom = 1,
	};
}

void InitPlayer() {
	// Allocating
	player = malloc(sizeof(Object));

	// Position and rotation
	player->pos = (Vector2){AREA_W/2, AREA_H/2};
	player->rot = 0;

	// Radius
	player->radius = PLAYER_RADIUS;

	// Vertices
	player->vertCount = 3;
	player->vertices   = malloc(player->vertCount * sizeof(Vector2));
	player->transVerts = malloc(player->vertCount * sizeof(Vector2));

	player->vertices[0] = (Vector2){           0, -PLAYER_SIZE},
	player->vertices[1] = (Vector2){ PLAYER_SIZE,  PLAYER_SIZE},
	player->vertices[2] = (Vector2){-PLAYER_SIZE,  PLAYER_SIZE},

	// Lifetime
	player->lifetime = NO_LIFETIME;

	// Type
	player->type = TYPE_PLAYER;

	// Health
	player->maxHealth = PLAYER_HEALTH_MAX;
	player->health = PLAYER_HEALTH;

	// Layer
	player->layer = LAYER_PLAYER;
	player->layerMask = LAYER_ASTEROID | LAYER_BASE | LAYER_ENEMY_PROJ;

	// Color
	player->color = WHITE;
}

void CreateAsteroid(Vector2 position, int radius) {
	// Creating object and appending node to objects list
	Node* astrNode = CreateObject();
	InsertToList(astrNode, &objs_head);

	Object* asteroid = astrNode->obj;

	// Position and radius
	asteroid->pos = position;
	asteroid->radius = radius;

	// Allocating vertices and transformed vertices array
	asteroid->vertCount = GetRandomValue(ASTEROID_MIN_VERTS, ASTEROID_MAX_VERTS);
	asteroid->vertices = malloc(asteroid->vertCount * sizeof(Vector2));
	asteroid->transVerts = malloc(asteroid->vertCount * sizeof(Vector2));

	// Positioning vertices
	for (int i = 0; i < asteroid->vertCount; ++i) {
		int dist = i == 0? asteroid->radius : asteroid->radius - GetRandomValue(0, ASTEROID_DISTORTION); // The first vertex will have the max radius
		float angle = (360*i/asteroid->vertCount)*DEG2RAD;
		asteroid->vertices[i] = Vector2Rotate((Vector2){0, -dist}, angle);
	}

	// Setting velocity
	float magnitude = GetRandomValue(ASTEROID_MIN_VEL, ASTEROID_MAX_VEL) / ((double)asteroid->radius/ASTEROID_VEL_SCALE_FACTOR);
	asteroid->vel = Vector2Rotate((Vector2){0, -magnitude}, GetRandomValue(0, PI*2));
	asteroid->spin = ASTEROID_ROT_SPEED/asteroid->radius;

	// Lifetime
	asteroid->lifetime = NO_LIFETIME;

	// Type
	asteroid->type = TYPE_ASTEROID;

	// Health
	asteroid->maxHealth = ASTEROID_MIN_HEALTH + ASTEROID_MAX_HEALTH * Normalize(radius, ASTEROID_DESTROY_SIZE, ASTEROID_MAX_SIZE);
	asteroid->health = asteroid->maxHealth;

	// Layer
	asteroid->layer = LAYER_ASTEROID;
	asteroid->layerMask = 0; // Asteroid collisions are checked by the colliding objects

	// Color
	asteroid->color = WHITE;
}

void CreateProjectile(int type, Vector2 pos) {
	// Creating object and appending node to lists
	Node* projNode = CreateObject();
	InsertToList(projNode, &objs_head);

	Object* proj = projNode->obj;
	
	// Transform
	proj->pos = pos;
	proj->rot = type == TYPE_PROJECTILE? player->rot : Vector2Angle((Vector2){0, -1}, Vector2Subtract(player->pos, pos));
	float projVel = type == TYPE_PROJECTILE? PROJECTILE_VEL : ENEMY_PROJ_VEL;
	proj->vel = Vector2Rotate((Vector2){0, -projVel}, proj->rot);

	// Radius
	proj->radius = PROJECTILE_RADIUS;

	// Vertices
	proj->vertCount = 1;
	proj->vertices = malloc(sizeof(Vector2));
	proj->transVerts = malloc(sizeof(Vector2));
	proj->vertices[0] = (Vector2){0, 0};

	// Lifetime
	proj->lifetime = type == TYPE_PROJECTILE? PROJECTILE_LIFETIME : ENEMY_PROJ_LIFETIME;

	// Type
	proj->type = type;

	// Health
	proj->maxHealth = PROJECTILE_HEALTH;
	proj->health = PROJECTILE_HEALTH;

	// Layer
	proj->layer = type == TYPE_PROJECTILE? LAYER_PROJECTILE : LAYER_ENEMY_PROJ;
	proj->layerMask = type == TYPE_PROJECTILE? LAYER_ASTEROID | LAYER_BASE : 0;

	// Color
	proj->color = type == TYPE_PROJECTILE? WHITE : RED;
}

Vector2* RegularPolygon(int vertCount, int radius) {
	Vector2* vertices = malloc(vertCount * sizeof(Vector2));
	for (int i = 0; i < vertCount; ++i) {
		int dist = radius;
		float angle = (i*360/vertCount)*DEG2RAD;
		vertices[i] = Vector2Rotate((Vector2){0, -dist}, angle);
	}
	return vertices;
}

void CreateEnemyBase() {
	// Creating object and appending node to lists
	Node* baseNode = CreateObject();
	InsertToList(baseNode, &objs_head);

	Object* base = baseNode->obj;

	// Radius
	base->radius = BASE_RADIUS;

	// Randomizing position
	Vector2 position;
	do {
		position = (Vector2){GetRandomValue(0, AREA_W), GetRandomValue(0, AREA_H)};
	} while (position.x + base->radius > player->pos.x - base->radius &&
		 position.x - base->radius < player->pos.x + base->radius &&
		 position.y + base->radius > player->pos.y - base->radius &&
		 position.y - base->radius < player->pos.y + base->radius); // Checking if it overlaps with the player

	// Transform
	base->pos = position;
	base->vel = (Vector2){0, 0};
	base->rot = 0;
	base->spin = 0;

	// Vertices
	base->vertCount = BASE_SIDES;
	base->vertices = RegularPolygon(base->vertCount, base->radius);
	base->transVerts = malloc(base->vertCount*sizeof(Vector2));

	// Lifetime
	base->lifetime = NO_LIFETIME;

	// Type
	base->type = TYPE_BASE;

	// Health
	base->maxHealth = BASE_HEALTH;
	base->health = BASE_HEALTH;

	// Layer
	base->layer = LAYER_BASE;
	base->layerMask = 0; // Enemy base collisions are checked by the colliding objects

	// Color
	base->color = RED;
}

void Initialize() {
	printf("Starting level: %d\n", level);

	if (!player) InitPlayer();

	objs_head = malloc(sizeof(Node));
	*objs_head = (Node){player, NULL, NULL};

	// Creating asteroids
	for (int i = 0; i < ASTEROID_COUNT_BASE + level * ASTEROID_COUNT_INCR; ++i) {
		// Randomizing max radius
		int rand1 = GetRandomValue(ASTEROID_MIN_SIZE, ASTEROID_MAX_SIZE);
		int rand2 = GetRandomValue(ASTEROID_MIN_SIZE, ASTEROID_MAX_SIZE);
		float radius = rand2 < rand1? rand2 : rand1; // Making it more likely for the radius to be small

		// Randomizing position
		Vector2 position;
		do {
			position = (Vector2){GetRandomValue(0, AREA_W), GetRandomValue(0, AREA_H)};
		} while (position.x + radius > player->pos.x - NO_ASTEROID_RADIUS &&
			 position.x - radius < player->pos.x + NO_ASTEROID_RADIUS &&
			 position.y + radius > player->pos.y - NO_ASTEROID_RADIUS &&
			 position.y - radius < player->pos.y + NO_ASTEROID_RADIUS); // Checking if it is inside the no asteroid radius around player

		CreateAsteroid(position, radius);
	}

	// Creating enemy bases
	int basesCount = level+1;
	for (int i = 0; i < basesCount; ++i) {
		CreateEnemyBase();
	}

	FreeBasesPos();
	basesPos = malloc(basesCount * sizeof(Vector2));
}

bool CheckCollision(Object* this, Object* other) {
	Object* obj = this->vertCount < other->vertCount? this : other; // We will iterate through the vertices of the object with least vertices
	Object* poly = obj == this? other : this; // We will use the the object with more vertices as the polygon in the collision check
	
	for (int i = 0; i < obj->vertCount; ++i) {
		if (CheckCollisionPointPoly(obj->transVerts[i], poly->transVerts, poly->vertCount)) return true;
	}

	return false;
}

void Process() {
	// - Main Menu -
	if (!player) {
		if (IsKeyPressed(KEY_P)) Initialize();
		return;
	}

	// - Game -
	float deltaTime = GetFrameTime();
	clock_t currClock = clock();

	// Player
	  // - Movement
	    // - Rotation
	player->spin = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A)) * PLAYER_ROT_SPEED;

	    // - Velocity
	float accel = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S)) * PLAYER_ACCEL * deltaTime;

	player->vel = Vector2Add(player->vel, Vector2Rotate((Vector2){0, -accel}, player->rot));
	player->vel = Vector2Subtract(player->vel, Vector2Scale(Vector2Normalize(player->vel), PLAYER_DEACCEL * deltaTime));

	player->vel = Vector2ClampValue(player->vel, 0, PLAYER_VEL_CAP);
	  //

	  // - Shooting
	if ((double)(currClock - lastShoot)/CLOCKS_PER_SEC > PLAYER_SHOOT_DELAY && IsKeyDown(KEY_SPACE)) {
		CreateProjectile(TYPE_PROJECTILE, Vector2Add(player->pos, Vector2Rotate((Vector2){0, -PROJECTILE_OFFSET}, player->rot)));
		lastShoot = currClock;
	}

	  // - Invulnerability indicator
	bool invul = (double)(currClock - lastHit)/CLOCKS_PER_SEC <= PLAYER_INVUL_SEC; // will be used later when checking hit
	player->color = invul? GRAY : WHITE;
	//

	// Going through all objects
	bool won = true;
	bool baseShot = false;

	Node* node = objs_head;
	while (node != NULL) {
		Object* obj = node->obj;

		// Enemy base shooting
		if (obj->type == TYPE_BASE) {
			won = false;
			if ((double)(currClock - lastBaseShoot)/CLOCKS_PER_SEC > BASE_SHOOT_DELAY) {
				CreateProjectile(TYPE_ENEMY_PROJ, obj->pos);
				baseShot = true;
			}
		}

		// Applying movement
		obj->rot += obj->spin * deltaTime;
		obj->pos = Vector2Add(obj->pos, Vector2Scale(obj->vel, deltaTime));

		// Wrapping
		if (obj->pos.x - obj->radius > AREA_W) obj->pos.x -= AREA_W + obj->radius*2;
		if (obj->pos.x + obj->radius < 0)      obj->pos.x += AREA_W + obj->radius*2;
		if (obj->pos.y - obj->radius > AREA_H) obj->pos.y -= AREA_H + obj->radius*2;
		if (obj->pos.y + obj->radius < 0)      obj->pos.y += AREA_H + obj->radius*2;

		// Getting transformed vertices
		for (int i = 0; i < obj->vertCount; ++i) {
			obj->transVerts[i] = Vector2Add(obj->pos, Vector2Rotate(obj->vertices[i], obj->rot));
		}

		// Lifetime
		if (obj->lifetime != NO_LIFETIME && (obj->lifetime -= deltaTime) < 0) {
			Node* next = node->next;
			DestroyNode(node, &objs_head);
			node = next;
			continue;
		}

		// Collision
		for (Node* other = objs_head; other != NULL; other = other->next) {
			Object* otherObj = other->obj;

			if (!(otherObj->layer & obj->layerMask)) continue; // Other object is not in the layer mask
			if (Vector2Distance(obj->pos, otherObj->pos) > obj->radius + otherObj->radius) continue; // Other object is not in range
			if (!CheckCollision(obj, otherObj)) continue; // The objects don't collide

			if (obj->type == TYPE_PLAYER && (otherObj->type == TYPE_ASTEROID || otherObj->type == TYPE_BASE || otherObj->type == TYPE_ENEMY_PROJ)) {
				// If player isn't invulnerable, damage player
				if (!invul) {
					--obj->health;
					lastHit = currClock;
				}

				// Knockback
				Vector2 knockDir = Vector2Normalize(Vector2Subtract(player->pos, otherObj->pos));
				player->vel = Vector2Scale(knockDir, PLAYER_KNOCKBACK);

				break;
			}

			if (obj->type == TYPE_PROJECTILE && (otherObj->type == TYPE_ASTEROID || otherObj->type == TYPE_BASE)) {
				--obj->health;
				--otherObj->health;
				break;
			}
		}

		// Health
		bool destroyed = false;
		if (obj->health <= 0) { // Object died
			if (obj->type == TYPE_ASTEROID && obj->radius/2 > ASTEROID_DESTROY_SIZE) {
				// If it's an asteroid and it's big enough, create two more
				CreateAsteroid(obj->pos, obj->radius/2);
				CreateAsteroid(obj->pos, obj->radius/2);
			} else if (obj->type == TYPE_PLAYER) {
				// If it's the player, lose
				puts("Lost! :(");
				if (level > highscore) highscore = level;
				level = 0;
				FreeObjects();
				return;
			}

			// Destroying
			Node* next = node->next;
			DestroyNode(node, &objs_head);
			node = next;
			destroyed = true;
		}
		//

		if (!destroyed) node = node->next;
	}

	if (baseShot) {
		lastBaseShoot = currClock;
	}

	// Move camera
	Vector2 newTarget;
	newTarget.x = Clamp(player->pos.x, WIDTH /2, AREA_W-(WIDTH /2));
	newTarget.y = Clamp(player->pos.y, HEIGHT/2, AREA_H-(HEIGHT/2));
	camera.target = newTarget;

	// Going to next level when there are no more enemy bases
	if (!won) return;
	if (player->health < player->maxHealth) ++player->health;
	++level;
	Initialize();
}

void Draw() {
	BeginDrawing();

	// Drawing stars
	BeginMode2D(camera);
	DrawTexture(starsTex, 0, 0, LIGHTGRAY);
	EndMode2D();

	// - Main Menu -
	if (!player) {
		// Highscore text
		int length = snprintf(NULL, 0, "HIGHSCORE: %d", highscore)+1; // +1 for null terminator
		char* highscoreText = malloc(length * sizeof(char));
		snprintf(highscoreText, length, "HIGHSCORE: %d", highscore);
		DrawText(highscoreText, 0, 0, FONT_SIZE, WHITE);
		free(highscoreText);

		// Play text
		const char* play = "PRESS P TO PLAY";
		int width = MeasureText(play, FONT_SIZE);
		DrawText(play, (WIDTH-width)/2, HEIGHT/2, FONT_SIZE, WHITE);

		// Controls text
		DrawText("WASD - MOVE", 0, HEIGHT-2*FONT_SIZE, FONT_SIZE, WHITE);
		DrawText("SPACE [HOLD] - SHOOT", 0, HEIGHT-FONT_SIZE, FONT_SIZE, WHITE);

		EndDrawing();
		return;
	}
	
	// - Game -
	// Level text
	int length = snprintf(NULL, 0, "LEVEL: %d", level)+1; // +1 for null terminator
	char* levelText = malloc(length * sizeof(char));
	snprintf(levelText, length, "LEVEL: %d", level);
	DrawText(levelText, 0, 0, FONT_SIZE, WHITE);
	free(levelText);

	// Health text
	length = snprintf(NULL, 0, "HEALTH: %d", player->health)+1; // +1 for null terminator
	char* healthText = malloc(length * sizeof(char));
	snprintf(healthText, length, "HEALTH: %d", player->health);
	DrawText(healthText, 0, HEIGHT-FONT_SIZE, FONT_SIZE, WHITE);
	free(healthText);

	BeginMode2D(camera);
	int baseCount = 0;
	Node* node = objs_head;
	while (node != NULL) {
		Object* obj = node->obj;

		// Drawing objects
		DrawObject(*obj);

		// Storing base positions
		if (obj->type == TYPE_BASE) basesPos[baseCount++] = obj->pos;

		node = node->next;
	}

	// Drawing arrows to indicate enemy base positions
	for (int i = 0; i < baseCount; ++i) {
		Vector2 diff = Vector2Subtract(basesPos[i], player->pos);

		Vector2 header = Vector2Normalize(diff);
		float angle = Vector2Angle((Vector2){0, -1}, header);
		Vector2 position = Vector2Add(player->pos, Vector2Scale(header, ARROW_DISTANCE));

		int vertCount = 3;
		Vector2* vertices = RegularPolygon(vertCount, ARROW_MAX_RADIUS);
		Vector2* transVerts = malloc(vertCount * sizeof(Vector2));

		for (int i = 0; i < vertCount; ++i) {
			transVerts[i] = Vector2Add(position, Vector2Rotate(vertices[i], angle));
		}

		DrawTriangleLines(transVerts[0], transVerts[1], transVerts[2], RED);

		free(vertices);
		free(transVerts);
	}
	//

	EndMode2D();
	EndDrawing();
}

void MainLoop() {
	Process();
	Draw();
}

int main() {
	OneTimeInit();

#ifndef PLATFORM_WEB
	SetTargetFPS(FPS);
	while (!WindowShouldClose()) {
		MainLoop();
	}
#else
	emscripten_set_main_loop(MainLoop, FPS, 1);
#endif

	exit(EXIT_SUCCESS);
}

