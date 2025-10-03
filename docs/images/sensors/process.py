import csv
import os

def load_sensor_data(filepath):
    """
    Loads sensor data, including bounding box coordinates, from a CSV file.

    Args:
        filepath (str): The path to the CSV file.

    Returns:
        list: A list of dictionaries, where each dictionary represents a sensor.
    """
    if not os.path.exists(filepath):
        print(f"Error: File not found at {filepath}")
        return []

    sensor_data = []
    
    # Define the required integer fields for bounding box coordinates
    int_fields = ['xmin', 'ymin', 'xmax', 'ymax']
    x = 129
    y = 285
    width = 165
    height = 165
    dx = 357
    dy = 230
    R=0
    C=0

    print(f"Loading data from {filepath}...")
    try:
        with open(filepath, mode='r', newline='', encoding='utf-8') as file:
            reader = csv.DictReader(file)
            for row in reader:
                # Convert coordinate strings to integers
                for field in int_fields:
                    try:
                        row[field] = int(row[field])
                    except ValueError:
                        print(f"Warning: Could not convert '{row[field]}' to integer for sensor '{row['sensor_name']}' in field '{field}'. Skipping row.")
                        continue
                
                # Calculate the area of the bounding box for demonstration
                row['xmin'] = x+dx*C
                row['xmax'] = x+dx*C+width
                row['ymin'] = y+dy*R
                row['ymax'] = y+dy*R+height
                width = row['xmax'] - row['xmin']
                height = row['ymax'] - row['ymin']
                row['bbox_area'] = width * height
                
                sensor_data.append(row)
                C+=1
                if C==6:
                    C=0
                    R+=1
        
        print(f"Successfully loaded {len(sensor_data)} sensor records.")
        return sensor_data

    except Exception as e:
        print(f"An error occurred while reading the CSV file: {e}")
        return []

def display_sensor_summary(data):
    """
    Prints a summary of the loaded sensor data.
    """
    if not data:
        print("No sensor data to display.")
        return

    print("\n--- Sensor Bounding Box Summary (Top 5) ---")
    
    for i, sensor in enumerate(data):
        print(f"\n{i+1}. Sensor: {sensor['category']} > {sensor['sensor_name']} ")
        print(f"   Coordinates (Relative): ({sensor['xmin']}, {sensor['ymin']}) to ({sensor['xmax']}, {sensor['ymax']})")

if __name__ == "__main__":
    # Ensure the CSV file is in the same directory as this script, or provide the full path.
    csv_file_path = 'sensor_locations.csv'
    
    # 1. Load the data
    sensors = load_sensor_data(csv_file_path)
    
    # 2. Display a summary of the processed data
    display_sensor_summary(sensors)

    # Example of how you might use this data with an image library (like PIL/Pillow):
    from PIL import Image, ImageDraw
     
    try:
        img = Image.open("sheet.jpg")
        draw = ImageDraw.Draw(img)
    
        for sensor in sensors:
            #Scale coordinates if necessary (e.g., if relative 0-1000 needs conversion)
            #Example: box = (sensor['xmin']*img.width//1000, sensor['ymin']*img.height//1000, ...)
            #For simple demonstration, use the absolute coordinates if the image is 1000x1000
            box = (sensor['xmin'], sensor['ymin'], sensor['xmax'], sensor['ymax'])
            
            #Draw a red rectangle around the area
            draw.rectangle(box, outline="red", width=2)
            
            #Crop a thumbnail (example using sensor #1)
            thumbnail = img.crop(box)
            thumbnail.save(f"thumbs/{sensor['sensor_name'].replace(' ', '_')}.jpg")
        
        img.save("image_with_bboxes.jpg")
        print("Bounding boxes drawn and saved to image_with_bboxes.jpg")
    
    except ImportError:
        print("\nNote: To draw bounding boxes or extract thumbnails, you'll need the Pillow library (pip install Pillow).")
        print("The coordinates are loaded successfully, but the visual processing step is commented out.")
    except FileNotFoundError:
        print("\nNote: Place your image file (e.g., IMG_20251003_094343.jpg) in the same directory to run the visual processing example.")
