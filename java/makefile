
CLASSES = vncviewer.class rfbProto.class authenticationPanel.class \
	  vncCanvas.class optionsFrame.class clipboardFrame.class \
	  animatedMemoryImageSource.class DesCipher.class

all: $(CLASSES) vncviewer.jar

%.class: %.java
	javac -O $<

vncviewer.jar: $(CLASSES)
	jar cf $@ $^
