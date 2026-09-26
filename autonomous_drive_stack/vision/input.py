class input():
    
    def __init__(self,image):
        self.image = image 
        
    def preprocess(self):
        origional = self.image.copy() 
        img = self.image #modify this for proprocessing 
        #TODO:code for preprocessing the image 
        return img, origional 