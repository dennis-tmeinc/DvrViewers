#ifndef __json_h__
#define __json_h__

/* Json Types: */
#define JSON_Null 0
#define JSON_Bool 1
#define JSON_Number 2
#define JSON_String 3
#define JSON_Array 4
#define JSON_Object 5

class json {
protected:
	json *next, *prev;
	json *child;
	char *name;
	int type;
	double valueNumber;
	char * valueString;

	

public:
	json() {
		next = prev = child = NULL;
		name = NULL;
		type = JSON_Null;
		valueString = NULL;
		valueNumber = 0.0;
	}

	// detach this from its parent first.
	~ json() {
		if (prev) prev->next = next;
		if (next) next->prev = prev;
		while (child) {
			delete Detach(0);
		}
		if (name) delete name;
		if (valueString) delete valueString;
	}

	static char * dupString(const * string) {
		int l = strlen(string);
		char * newstring = new char[l + 1];
		strcpy(newstring, string);
		return newstring;
	}

	static json * createNull()
	{
		return new json;
	}

	static json * createBool(const char * name, int bvalue )
	{
		json * _j = new json;
		_j->type = JSON_Bool;
		if( name )
			_j->name = dupString(name);
		_j->valueNumber = bvalue;
		return _j;
	}

	static json * createNumber(const char * name, double nvalue)
	{
		json * _j = new json;
		_j->type = JSON_Number;
		if (name)
			_j->name = dupString(name);
		_j->valueNumber = nvalue;
		return _j;
	}

	static json * createString(const char * name, const char * string)
	{
		json * _j = new json;
		_j->type = JSON_String;
		if (name)
			_j->name = dupString(name);
		if( string )
			_j->valueString = dupString(string)
		return _j;
	}

	static json * createArray(const char * name)
	{
		json * _j = new json;
		_j->type = JSON_Array;
		if (name)
			_j->name = dupString(name);
		return _j;
	}

	static json * createObject(const char * name)
	{
		json * _j = createArray(name);
		_j->type = JSON_Object;
		return _j ;
	}

	static json * decode(const char * jsontext)
	{

	}

	// return bytes written
	int encode( char * buf, int bsize ) {

	}

	double GetNumber() {
		return valueNumber;
	}

	char * GetString() {
		return valueString;
	}

	int    GetInt() {
		return (int)GetNumber();
	}

	int		GetBool() {
		return GetInt() != 0;
	}

	void SetNumber(double nvalue) {
		valueNumber = nvalue;
	}

	void SetString(const char * string) {
		valueString = dupString(string);
	}
	
	void    SetInt( int value ) {
		SetNumber((double)value);
	}

	void    SetBool(int bvalue) {
		SetInt(bvalue);
	}

	int ArraySize() {
		int s = 0;
		json * ch = child ;
		while (ch) {
			s++;
			ch = ch->next;
		}
		return s;
	}

	json *GetArrayItem(int idx) {
		json * ch = child;
		while ( ch && idx-->0) {
			ch = ch->next;
		}
		return ch ;
	}

	json *GetObjectItem(const char *name) {
		json * ch = child;
		while (ch ) {
			if (strcmp(name, ch->name) == 0) {
				return ch;
			}
			ch = ch->next;
		}
		return NULL;
	}

	void AddItem(json * item) {
		if (child == NULL) {
			child = item;
			item->prev = item->next = NULL;
		}
		else {
			json * ch = child;
			while (ch->next) {
				ch = ch->next;
			}
			ch->next = item;
			item->prev = ch;
			item->next = NULL;
		}
	}

	json * Detach(int idx) {
		json * ch = GetArrayItem(idx);
		if (ch) {
			if (ch == child) {
				child = ch->next;
			}
			else {
				ch->prev->next = ch->next;
				ch->next->prev = ch->prev;
			}
			ch->next = ch->prev = NULL;
		}
		return ch;
	}

	json * Detach(char * name) {
		json * ch = GetObjectItem(name);
		if (ch) {
			if (ch == child) {
				child = ch->next;
			}
			else {
				ch->prev->next = ch->next;
				ch->next->prev = ch->prev;
			}
			ch->next = ch->prev = NULL;
		}
		return ch;
	}
};

typedef struct cJSON {
	struct cJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	

	char *valuestring;			/* The item's string, if type==cJSON_String */
	int valueint;				/* The item's number, if type==cJSON_Number */
	double valuedouble;			/* The item's number, if type==cJSON_Number */

	char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} cJSON;

#define cJSON_malloc malloc
#define cJSON_free free

/* These calls create a cJSON item of the appropriate type. */
extern cJSON *cJSON_CreateNull(void);
extern cJSON *cJSON_CreateBool(int b);
extern cJSON *cJSON_CreateNumber(double num);
extern cJSON *cJSON_CreateString(const char *string);
extern cJSON *cJSON_CreateArray(void);
extern cJSON *cJSON_CreateObject(void);


/* Supply a block of JSON, and this returns a cJSON object you can interrogate. Call cJSON_Delete when finished. */
extern cJSON *cJSON_Parse(const char *value);
/* Render a cJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *cJSON_Print(cJSON *item);
/* Render a cJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *cJSON_PrintUnformatted(cJSON *item);
/* Delete a cJSON entity and all subentities. */
extern void   cJSON_Delete(cJSON *c);

/* Returns the number of items in an array (or object). */
extern int	  cJSON_GetArraySize(cJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern cJSON *cJSON_GetArrayItem(cJSON *array,int item);
/* Get item "string" from object. Case insensitive. */
extern cJSON *cJSON_GetObjectItem(cJSON *object,const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */
extern const char *cJSON_GetErrorPtr(void);
	
/* These calls create a cJSON item of the appropriate type. */
extern cJSON *cJSON_CreateNull(void);
extern cJSON *cJSON_CreateTrue(void);
extern cJSON *cJSON_CreateFalse(void);
extern cJSON *cJSON_CreateBool(int b);
extern cJSON *cJSON_CreateNumber(double num);
extern cJSON *cJSON_CreateString(const char *string);
extern cJSON *cJSON_CreateArray(void);
extern cJSON *cJSON_CreateObject(void);

/* Remove/Detatch items from Arrays/Objects. */
extern cJSON *cJSON_DetachItemFromArray(cJSON *array,int which);
extern void   cJSON_DeleteItemFromArray(cJSON *array,int which);
extern cJSON *cJSON_DetachItemFromObject(cJSON *object,const char *string);
extern void   cJSON_DeleteItemFromObject(cJSON *object,const char *string);


#endif		// __json_h__
