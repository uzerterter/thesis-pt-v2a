"""
X-CLIP Encoder wrapper for video and text encoding.
"""

import io
import logging
from pathlib import Path
from typing import Union, List
import tempfile

import torch
import numpy as np
from transformers import AutoModel, AutoProcessor
from PIL import Image
import av

logger = logging.getLogger(__name__)


class XCLIPEncoder:
    """X-CLIP encoder for video and text features"""
    
    def __init__(self, model_name: str = "microsoft/xclip-base-patch32", device: str = "cuda"):
        """
        Initialize X-CLIP encoder.
        
        Args:
            model_name: Hugging Face model name
            device: Device to run on ('cuda' or 'cpu')
        """
        self.device = device
        self.model_name = model_name
        
        logger.info(f"Loading X-CLIP model: {model_name}")
        self.processor = AutoProcessor.from_pretrained(model_name)
        self.model = AutoModel.from_pretrained(model_name).to(device)
        self.model.eval()
        
        logger.info(f"✓ X-CLIP loaded on {device}")
        
        # Determine image size based on model (base=224, large=336)
        self.image_size = 336 if "large" in model_name.lower() else 224
        
        # Get embedding dimension
        with torch.no_grad():
            dummy_text = self.processor(text=["test"], return_tensors="pt", padding=True)
            dummy_output = self.model.get_text_features(**{k: v.to(device) for k, v in dummy_text.items()})
            self.embedding_dim = dummy_output.shape[-1]
        
        logger.info(f"Embedding dimension: {self.embedding_dim}")
        logger.info(f"Image size: {self.image_size}x{self.image_size}")
    
    def encode_text(self, text: Union[str, List[str]]) -> np.ndarray:
        """
        Encode text to embedding.
        
        Args:
            text: Single text string or list of strings
        
        Returns:
            Normalized embedding vector(s) as numpy array
        """
        if isinstance(text, str):
            text = [text]
        
        with torch.no_grad():
            inputs = self.processor(text=text, return_tensors="pt", padding=True)
            inputs = {k: v.to(self.device) for k, v in inputs.items()}
            
            text_features = self.model.get_text_features(**inputs)
            
            # Normalize
            text_features = text_features / text_features.norm(dim=-1, keepdim=True)
            
            # Return as numpy
            embeddings = text_features.cpu().numpy()
            
            # Return single vector if single text input
            if len(text) == 1:
                return embeddings[0]
            return embeddings
    
    async def encode_video(self, video_file, num_frames: int = 8) -> np.ndarray:
        """
        Encode video to embedding.
        
        Args:
            video_file: UploadFile object or path to video
            num_frames: Number of frames to sample (default: 8)
        
        Returns:
            Normalized embedding vector as numpy array
        """
        # Handle UploadFile (FastAPI)
        if hasattr(video_file, 'read'):
            video_bytes = await video_file.read()
            # Write to temporary file for av to read
            with tempfile.NamedTemporaryFile(suffix='.mp4', delete=False) as tmp:
                tmp.write(video_bytes)
                tmp_path = tmp.name
            
            try:
                frames = self._extract_frames(tmp_path, num_frames)
            finally:
                Path(tmp_path).unlink()
        else:
            # Assume it's a path
            frames = self._extract_frames(str(video_file), num_frames)
        
        # Encode frames with model-specific image size
        with torch.no_grad():
            # Process with correct size - need both resize and crop for square images
            inputs = self.processor(
                videos=list(frames), 
                return_tensors="pt",
                do_resize=True,
                size={"shortest_edge": self.image_size},
                do_center_crop=True,
                crop_size={"height": self.image_size, "width": self.image_size}
            )
            inputs = {k: v.to(self.device) for k, v in inputs.items()}
            
            video_features = self.model.get_video_features(**inputs)
            
            # Normalize
            video_features = video_features / video_features.norm(dim=-1, keepdim=True)
            
            # Return as numpy
            return video_features[0].cpu().numpy()
    
    def _extract_frames(self, video_path: str, num_frames: int = 8) -> List[Image.Image]:
        """
        Extract uniformly sampled frames from video.
        
        Args:
            video_path: Path to video file
            num_frames: Number of frames to extract
        
        Returns:
            List of PIL Images
        """
        container = av.open(video_path)
        video_stream = container.streams.video[0]
        
        # Get total frames
        total_frames = video_stream.frames
        if total_frames == 0:
            # Fallback: estimate from duration and fps
            total_frames = int(video_stream.duration * video_stream.time_base * video_stream.average_rate)
        
        # Calculate frame indices to sample
        indices = np.linspace(0, total_frames - 1, num_frames, dtype=int)
        
        frames = []
        frame_idx = 0
        
        for frame in container.decode(video=0):
            if frame_idx in indices:
                # Convert to PIL Image
                img = frame.to_image()
                frames.append(img)
                
                if len(frames) >= num_frames:
                    break
            
            frame_idx += 1
        
        container.close()
        
        if len(frames) < num_frames:
            logger.warning(f"Only extracted {len(frames)}/{num_frames} frames")
        
        return frames
    
    def get_embedding_dim(self) -> int:
        """Get the embedding dimension"""
        return self.embedding_dim
