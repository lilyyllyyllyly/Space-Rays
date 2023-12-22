#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "object.h"

void FreeNode(Node* node) {
	free(node->obj->vertices);
	free(node->obj);
	free(node);
}

// Unlinks nodes from list starting at Node head and then frees
void DestroyNode(Node* node, Node** head) {
	// Unlinking node
	if (node->next) node->next->prev = node->prev;
	if (node->prev) node->prev->next = node->next;
	else *head = node->next;

	// Freeing
	FreeNode(node);
}

// adds the Node node to the start of the list starting at head
// node: node to insert
// head: first element of the list
void InsertToList(Node* node, Node** head) {
	(*head)->prev = node;
	node->next = *head;
	*head = node;
}

// returns: node of the object
Node* CreateObject() {
	// Allocating memory for object
	Object* obj = malloc(sizeof(Object));

	// Creating node for the object
	Node* objNode = malloc(sizeof(Node));
	*objNode = (Node){obj, NULL, NULL};

	return objNode;
}

void DrawObject(Object obj) {
	// Draw point (if only one vertex)
	if (obj.vertCount == 1) {
		DrawCircleV(obj.transVerts[0], obj.radius, WHITE);
		return;
	}

	// Drawing lines (multiple vertices)
	for (int i = 0; i < obj.vertCount; ++i) {
		if (i == 0) {
			DrawLineV(obj.transVerts[obj.vertCount-1], obj.transVerts[i], WHITE);
			continue;
		}

		DrawLineV(obj.transVerts[i], obj.transVerts[i-1], WHITE);
	}
}

