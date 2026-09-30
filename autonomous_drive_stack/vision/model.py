from ultralytics import YOLO
class model():
    def __init__(self ):
        pass
        
    def process(self):
        
        # Load a COCO-pretrained YOLOv8m model
        #todo put yolo model 
        model = YOLO("file path to model")
        
        #load image 
        results = model.predict(r"file to video", conf = .5)
        
        #show result 
        results[0].show()
        

        
    

    
    
        