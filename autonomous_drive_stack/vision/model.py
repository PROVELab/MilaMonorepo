from ultralytics import YOLO
class model():
    def __init__(self ):
        pass
        
    def process(self):
        
        # Load a COCO-pretrained YOLOv8m model
        #todo put yolo model 
        model = YOLO(r"C:\Users\tj675\Downloads\yolov8n.pt")
        
        #load image 
        results = model.predict(r"C:\Users\tj675\Downloads\test1.jpg", conf = .5)
        
        return results
                
        
    def bounding_box_cordinates(self, results):
        for result in results:
            for box in result.boxes:
                x1, y1, x2, y2  = box.xyxy[0].int().tolist()  # Bounding box coordinates
                print(f" object: {result.names[box.cls.item()]}, Coordinates: {x1}, {y1}, {x2}, {y2}")
                
    def display_result(self, results):
        #show result 
        results[0].show()
            
        
        
        
if __name__ == "__main__":
        
    Model = model()   
    results = Model.process()
    Model.bounding_box_cordinates(results)
    Model.display_result(results)
       
        
        
        

        
    

    
    
        