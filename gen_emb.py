import nltk
import json
import multiprocessing as mp
from openai import OpenAI
from dotenv import load_dotenv, find_dotenv
from concurrent.futures import ThreadPoolExecutor

load_dotenv(find_dotenv())
nltk.download('punkt')
client = OpenAI()


def split_text_into_sentences(text):
    sentences = nltk.sent_tokenize(text)
    return sentences


def read_txt_file(file_path):
    with open(file_path, "r") as file:
        return file.read()
    

def get_openai_embedding(text, model="text-embedding-3-small"):
   text = text.replace("\n", " ")
   return client.embeddings.create(input = [text], model=model).data[0].embedding[:5]


def write_embeddings_to_file(embeddings_list):
    with open("embeddings.txt", "w") as file:
        file.write(json.dumps(embeddings_list))

def gen_embedding(sentence, idx):
    print(f"Processing sentence with id: {idx}")
    vector_embedding = get_openai_embedding(sentence)
    return {
        "id": idx,
        "token": sentence,
        "embedding": vector_embedding
    }


if __name__ == "__main__":
    document = read_txt_file("papers_data/papers.txt")
    sentences = split_text_into_sentences(document)
    print(f"Total sentences: {len(sentences)}")

    sentences = sentences[:10]

    embeddings_list = list()
    with ThreadPoolExecutor(max_workers=mp.cpu_count()) as executor:
        futures_to_openai = {executor.submit(gen_embedding, sentence, idx): idx for (idx, sentence) in enumerate(sentences)}
        for future in futures_to_openai:
            embeddings_list.append(future.result())

    write_embeddings_to_file(embeddings_list)
