#ifndef OBJECT_H
#define OBJECT_H

typedef struct {
	Vector2 pos;
	Vector2 vel;

	float rot;
	float spin; // Spin angular velocity, in rad/sec

	int vertCount;
	Vector2* vertices;
	Vector2* transVerts; // Transformed vertices (with position and rotation applied)

	int radius; // Radius when wrapping and checking for collision
	
	float lifetime; // Time in seconds for object to be destroyed, NO_LIFETIME means it won't be
	
	int type;

	int health;
	int maxHealth;

	// Collision layers
	char layer;
	char layerMask;
} Object;

struct List;

// Node in a doubly-linked list of objects
typedef struct Node {
	Object* obj;
	struct Node* next;
	struct Node* prev;
} Node;

void FreeNode(Node* node);

// Unlinks Node node from list starting at Node head and then frees
void DestroyNode(Node* node, Node** head);

// adds the Node node to the start of the list starting at head
// node: node to insert
// head: first element of the list
void InsertToList(Node* node, Node** head);

// returns: node of the object
Node* CreateObject();

void DrawObject(Object obj);

#endif

