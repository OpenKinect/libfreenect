
LDFLAGS += -lusb-1.0 -lGL -lGLU -lglut -pthread

OBJECTS = cameras.o main.o

testcam : $(OBJECTS)
	gcc $(LDFLAGS) -o $@ $(OBJECTS)

inits.h : inits.txt geninits.py
	python geninits.py $< > $@

clean:
	rm -f testcam $(OBJECTS) inits.h