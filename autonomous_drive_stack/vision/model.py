from ultralytics import YOLO
class model():
    def __init__(self ):
        self.model = YOLO(r"model path ")
        
        
    def process(self):
        
        #TODO: tune conf and iou 
        results = self.model.track(r"video path", show = True, conf = 0.5, iou = 0.3, persist = True, tracker = "bytetrack.yaml")
        
        return results
                
        
    def bounding_box_cordinates(self, results):
        detection = {}
        for result in results:
            for box in result.boxes:
                x1, y1, x2, y2  = box.xyxy[0].int().tolist()  # Bounding box coordinates
                conf = box.conf.item()
                track_id = box.id.item()
                class_id = box.cls.item()
                detection.update( {int(track_id): [self.model.names[class_id], conf, x1,y1,x2,y2]})
        
        return detection
                
            
        
        
        

       
        
        
        

        
    

    
    
        