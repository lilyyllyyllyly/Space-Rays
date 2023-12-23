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

// Player
#define PLAYER_ACCEL 300.0
#define PLAYER_DEACCEL 25.0
#define PLAYER_VEL_CAP 400.0
#define PLAYER_ROT_SPEED 5.0
#define PLAYER_SIZE 15
#define PLAYER_SHOOT_DELAY 0.15

// Asteroid
#define ASTEROID_COUNT_BASE 5
#define ASTEROID_COUNT_INCR 2 // increment

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

// Projectile
#define PROJECTILE_RADIUS 2
#define PROJECTILE_OFFSET 10
#define PROJECTILE_VEL 500.0
#define PROJECTILE_LIFETIME 0.7

// Object types
#define TYPE_PLAYER     0
#define TYPE_ASTEROID   1
#define TYPE_PROJECTILE 2
//

#define NO_ASTEROID_RADIUS 130 // Radius around the player where asteroids can't spawn
#define NO_LIFETIME -1

// First element of list of all objects
Node* objs_head = NULL;

Object* player = NULL;
clock_t lastShoot;

int level = 0;

void OnInterrupt(int signal) {
	puts("\nProgram terminated by SIGINT. Exiting.");
	exit(EXIT_SUCCESS);
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

void OneTimeInit() {
	signal(SIGINT, OnInterrupt);

	SetTraceLogLevel(LOG_WARNING); /* getting rid of annoying init info */
	InitWindow(WIDTH, HEIGHT, "asteroids :3");
	atexit(CloseWindow);

	atexit(FreeObjects);

	lastShoot = clock();
}

void InitPlayer() {
	// Allocating
	player = malloc(sizeof(Object));

	// Position and rotation
	player->pos = (Vector2){WIDTH/2, HEIGHT/2};
	player->rot = 0;

	// Vertices
	player->vertCount = 3;
	player->vertices = malloc(player->vertCount * sizeof(Vector2));
	player->transVerts = malloc(player->vertCount * sizeof(Vector2));
	player->vertices[0] = (Vector2){  0, -10};
	player->vertices[1] = (Vector2){ 10,  10};
	player->vertices[2] = (Vector2){-10,  10};

	// Radius
	player->radius = PLAYER_SIZE;

	// Lifetime
	player->lifetime = NO_LIFETIME;

	// Type
	player->type = TYPE_PLAYER;
}

void CreateAsteroid(Vector2 position, float radius) {
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
}

void CreateProjectile() {
	// Creating object and appending node to lists
	Node* projNode = CreateObject();
	InsertToList(projNode, &objs_head);

	Object* proj = projNode->obj;
	
	// Transform
	proj->pos = Vector2Add(player->pos, Vector2Rotate((Vector2){0, -PROJECTILE_OFFSET}, player->rot));
	proj->rot = player->rot;
	proj->vel = Vector2Rotate((Vector2){0, -PROJECTILE_VEL}, proj->rot);

	// Radius
	proj->radius = PROJECTILE_RADIUS;

	// Vertices
	proj->vertCount = 1;
	proj->vertices = malloc(sizeof(Vector2));
	proj->transVerts = malloc(sizeof(Vector2));
	proj->vertices[0] = (Vector2){0, 0};

	// Lifetime
	proj->lifetime = PROJECTILE_LIFETIME;

	// Type
	proj->type = TYPE_PROJECTILE;
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
			position = (Vector2){GetRandomValue(0, WIDTH), GetRandomValue(0, HEIGHT)};
		} while (position.x + radius > player->pos.x - NO_ASTEROID_RADIUS &&
			 position.x - radius < player->pos.x + NO_ASTEROID_RADIUS &&
			 position.y + radius > player->pos.y - NO_ASTEROID_RADIUS &&
			 position.y - radius < player->pos.y + NO_ASTEROID_RADIUS); // Checking if it is inside the no asteroid radius around player
												   //
		CreateAsteroid(position, radius);
	}
}

bool AsteroidPlayerCollision(Object* astr) {
	if (Vector2Distance(astr->pos, player->pos) > astr->radius + player->radius) return false; // Not in range

	// Checking if one of the player's vertices collides with the asteroid
	for (int i = 0; i < player->vertCount; ++i) {
		if (CheckCollisionPointPoly(player->transVerts[i], astr->transVerts, astr->vertCount)) {
			return true;
		}
	}

	return false;
}

bool AsteroidProjectileCollision(Object* astr, Node** projNode) {
	// Going through each projectile to check for collision
	*projNode = objs_head;
	while (*projNode != NULL) {
		Object* proj = (*projNode)->obj;
		if (proj->type != TYPE_PROJECTILE) goto next; // Not an projectile

		if (Vector2Distance(astr->pos, proj->pos) > astr->radius) goto next; // Not in range

		if (CheckCollisionPointPoly(proj->pos, astr->transVerts, astr->vertCount)) {
			return true;
		}
		
next:
		*projNode = (*projNode)->next;
	}

	return false;
}

void Process() {
	float deltaTime = GetFrameTime();

	// Player
	  // - Movement
	    // - Rotation
	player->spin = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A)) * PLAYER_ROT_SPEED;

	    // - Velocity
	float accel = IsKeyDown(KEY_W) * PLAYER_ACCEL * deltaTime;

	player->vel = Vector2Add(player->vel, Vector2Rotate((Vector2){0, -accel}, player->rot));
	player->vel = Vector2Subtract(player->vel, Vector2Scale(Vector2Normalize(player->vel), PLAYER_DEACCEL * deltaTime));

	player->vel = Vector2ClampValue(player->vel, 0, PLAYER_VEL_CAP);
	  //

	  // - Shooting
	clock_t currClock = clock();
	if ((double)(currClock - lastShoot)/CLOCKS_PER_SEC > PLAYER_SHOOT_DELAY && IsKeyDown(KEY_SPACE)) {
		CreateProjectile();
		lastShoot = currClock;
	}
	//

	// Going through all objects
	bool won = true;

	Node* node = objs_head;
	while (node != NULL) {
		Object* obj = node->obj;
		if (obj->type == TYPE_ASTEROID) won = false;

		// Applying movement
		  // - Moving and rotating
		obj->rot += obj->spin * deltaTime;
		obj->pos = Vector2Add(obj->pos, Vector2Scale(obj->vel, deltaTime));

		  // - Wrapping
		if (obj->pos.x - obj->radius > WIDTH)  obj->pos.x -= WIDTH  + obj->radius*2;
		if (obj->pos.x + obj->radius < 0)      obj->pos.x += WIDTH  + obj->radius*2;
		if (obj->pos.y - obj->radius > HEIGHT) obj->pos.y -= HEIGHT + obj->radius*2;
		if (obj->pos.y + obj->radius < 0)      obj->pos.y += HEIGHT + obj->radius*2;
		//

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
		if (obj->type != TYPE_ASTEROID) goto next; // We only care about asteroid collisions

		  // - Player x Asteroid
		if (AsteroidPlayerCollision(obj)) {
			// Restart
			puts("Lost! :(");
			level = 0;
			FreeObjects();
			Initialize();
			return;
		}

		  // - Projectile x Asteroid
		Node* proj = NULL;
		if (AsteroidProjectileCollision(obj, &proj)) {
			// Destroy projectile
			DestroyNode(proj, &objs_head);

			// Create two more asteroids
			if (obj->radius/2 > ASTEROID_DESTROY_SIZE) {
				CreateAsteroid(obj->pos, obj->radius/2);
				CreateAsteroid(obj->pos, obj->radius/2);
			}
			
			// Destroy original asteroid
			Node* next = node->next;
			DestroyNode(node, &objs_head);
			node = next;
			continue;
		}
		//

next:
		node = node->next;
	}

	// Going to next level when there are no more asteroids
	if (!won) return;
	++level;
	Initialize();
}

void Draw() {
	BeginDrawing();
	ClearBackground(BLACK);

	Node* node = objs_head;
	while (node != NULL) {
		DrawObject(*(node->obj));
		node = node->next;
	}

	EndDrawing();
}

void MainLoop() {
	Process();
	Draw();
}

int main() {
	OneTimeInit();
	Initialize();

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

